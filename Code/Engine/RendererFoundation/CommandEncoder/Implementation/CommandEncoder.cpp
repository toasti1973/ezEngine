#include <RendererFoundationPCH.h>

#include <RendererFoundation/CommandEncoder/CommandEncoder.h>
#include <RendererFoundation/Device/Device.h>
#include <RendererFoundation/Resources/Buffer.h>
#include <RendererFoundation/Resources/Query.h>
#include <RendererFoundation/Resources/RenderTargetView.h>
#include <RendererFoundation/Resources/ResourceView.h>
#include <RendererFoundation/Resources/Texture.h>
#include <RendererFoundation/Resources/UnorderedAccesView.h>

ezGALCommandEncoder::ezGALCommandEncoder(ezGALDevice& device)
  : m_Device(device)
{
  InvalidateState();
}

ezGALCommandEncoder::~ezGALCommandEncoder() = default;

void ezGALCommandEncoder::SetShader(ezGALShaderHandle hShader)
{
  AssertRenderingThread();
  /// \todo Assert for shader capabilities (supported shader stages etc.)

  if (m_hShader == hShader)
  {
    CountRedundantStateChange();
    return;
  }

  const ezGALShader* pShader = m_Device.GetShader(hShader);
  EZ_ASSERT_DEV(pShader != nullptr, "The given shader handle isn't valid, this may be a use after destroy!");

  SetShaderPlatform(pShader);

  m_hShader = hShader;
  CountStateChange();
}

void ezGALCommandEncoder::SetConstantBuffer(ezUInt32 uiSlot, ezGALBufferHandle hBuffer)
{
  AssertRenderingThread();
  EZ_ASSERT_RELEASE(uiSlot < EZ_GAL_MAX_CONSTANT_BUFFER_COUNT, "Constant buffer slot index too big!");

  if (m_hConstantBuffers[uiSlot] == hBuffer)
  {
    CountRedundantStateChange();
    return;
  }

  const ezGALBuffer* pBuffer = m_Device.GetBuffer(hBuffer);
  EZ_ASSERT_DEV(pBuffer == nullptr || pBuffer->GetDescription().m_BufferType == ezGALBufferType::ConstantBuffer, "Wrong buffer type");

  SetConstantBufferPlatform(uiSlot, pBuffer);

  m_hConstantBuffers[uiSlot] = hBuffer;

  CountStateChange();
}

void ezGALCommandEncoder::SetSamplerState(ezGALShaderStage::Enum Stage, ezUInt32 uiSlot, ezGALSamplerStateHandle hSamplerState)
{
  AssertRenderingThread();
  EZ_ASSERT_RELEASE(uiSlot < EZ_GAL_MAX_SAMPLER_COUNT, "Sampler state slot index too big!");

  if (m_hSamplerStates[Stage][uiSlot] == hSamplerState)
  {
    CountRedundantStateChange();
    return;
  }

  const ezGALSamplerState* pSamplerState = m_Device.GetSamplerState(hSamplerState);

  SetSamplerStatePlatform(Stage, uiSlot, pSamplerState);

  m_hSamplerStates[Stage][uiSlot] = hSamplerState;

  CountStateChange();
}

void ezGALCommandEncoder::SetResourceView(ezGALShaderStage::Enum Stage, ezUInt32 uiSlot, ezGALResourceViewHandle hResourceView)
{
  AssertRenderingThread();

  /// \todo Check if the device supports the stage / the slot index

  auto& boundResourceViews = m_hResourceViews[Stage];
  if (uiSlot < boundResourceViews.GetCount() && boundResourceViews[uiSlot] == hResourceView)
  {
    CountRedundantStateChange();
    return;
  }

  const ezGALResourceView* pResourceView = m_Device.GetResourceView(hResourceView);
  if (pResourceView != nullptr)
  {
    if (UnsetUnorderedAccessViews(pResourceView->GetResource()))
    {
      FlushPlatform();
    }
  }

  SetResourceViewPlatform(Stage, uiSlot, pResourceView);

  boundResourceViews.EnsureCount(uiSlot + 1);
  boundResourceViews[uiSlot] = hResourceView;

  auto& boundResources = m_pResourcesForResourceViews[Stage];
  boundResources.EnsureCount(uiSlot + 1);
  boundResources[uiSlot] = pResourceView != nullptr ? pResourceView->GetResource()->GetParentResource() : nullptr;

  CountStateChange();
}

#if 0
void ezGALCommandEncoder::SetRenderTargetSetup(const ezGALRenderTargetSetup& RenderTargetSetup)
{
  AssertRenderingThread();

  if (m_RenderTargetSetup == RenderTargetSetup)
  {
    CountRedundantStateChange();
    return;
  }

  const ezGALRenderTargetView* pRenderTargetViews[EZ_GAL_MAX_RENDERTARGET_COUNT] = {nullptr};
  const ezGALRenderTargetView* pDepthStencilView = nullptr;

  ezUInt32 uiRenderTargetCount = 0;

  bool bFlushNeeded = false;

  if (RenderTargetSetup.HasRenderTargets())
  {
    for (ezUInt8 uiIndex = 0; uiIndex <= RenderTargetSetup.GetMaxRenderTargetIndex(); ++uiIndex)
    {
      const ezGALRenderTargetView* pRenderTargetView = m_Device.GetRenderTargetView(RenderTargetSetup.GetRenderTarget(uiIndex));
      if (pRenderTargetView != nullptr)
      {
        const ezGALResourceBase* pTexture = pRenderTargetView->GetTexture()->GetParentResource();

        bFlushNeeded |= UnsetResourceViews(pTexture);
        bFlushNeeded |= UnsetUnorderedAccessViews(pTexture);
      }

      pRenderTargetViews[uiIndex] = pRenderTargetView;
    }

    uiRenderTargetCount = RenderTargetSetup.GetMaxRenderTargetIndex() + 1;
  }

  pDepthStencilView = m_Device.GetRenderTargetView(RenderTargetSetup.GetDepthStencilTarget());
  if (pDepthStencilView != nullptr)
  {
    const ezGALResourceBase* pTexture = pDepthStencilView->GetTexture()->GetParentResource();

    bFlushNeeded |= UnsetResourceViews(pTexture);
    bFlushNeeded |= UnsetUnorderedAccessViews(pTexture);
  }

  if (bFlushNeeded)
  {
    FlushPlatform();
  }
  SetRenderTargetSetupPlatform(ezMakeArrayPtr(pRenderTargetViews, uiRenderTargetCount), pDepthStencilView);

  m_RenderTargetSetup = RenderTargetSetup;

  CountStateChange();
}
#endif

void ezGALCommandEncoder::SetUnorderedAccessView(ezUInt32 uiSlot, ezGALUnorderedAccessViewHandle hUnorderedAccessView)
{
  AssertRenderingThread();

  /// \todo Check if the device supports the stage / the slot index

  if (uiSlot < m_hUnorderedAccessViews.GetCount() && m_hUnorderedAccessViews[uiSlot] == hUnorderedAccessView)
  {
    CountRedundantStateChange();
    return;
  }

  const ezGALUnorderedAccessView* pUnorderedAccessView = m_Device.GetUnorderedAccessView(hUnorderedAccessView);
  if (pUnorderedAccessView != nullptr)
  {
    if (UnsetResourceViews(pUnorderedAccessView->GetResource()))
    {
      FlushPlatform();
    }
  }

  SetUnorderedAccessViewPlatform(uiSlot, pUnorderedAccessView);

  m_hUnorderedAccessViews.EnsureCount(uiSlot + 1);
  m_hUnorderedAccessViews[uiSlot] = hUnorderedAccessView;

  m_pResourcesForUnorderedAccessViews.EnsureCount(uiSlot + 1);
  m_pResourcesForUnorderedAccessViews[uiSlot] =
    pUnorderedAccessView != nullptr ? pUnorderedAccessView->GetResource()->GetParentResource() : nullptr;

  CountStateChange();
}

void ezGALCommandEncoder::InsertFence(ezGALFenceHandle hFence)
{
  AssertRenderingThread();

  InsertFencePlatform(m_Device.GetFence(hFence));
}

bool ezGALCommandEncoder::IsFenceReached(ezGALFenceHandle hFence)
{
  AssertRenderingThread();

  return IsFenceReachedPlatform(m_Device.GetFence(hFence));
}

void ezGALCommandEncoder::WaitForFence(ezGALFenceHandle hFence)
{
  AssertRenderingThread();

  WaitForFencePlatform(m_Device.GetFence(hFence));
}

void ezGALCommandEncoder::BeginQuery(ezGALQueryHandle hQuery)
{
  AssertRenderingThread();

  auto query = m_Device.GetQuery(hQuery);
  EZ_ASSERT_DEV(!query->m_bStarted, "Can't stat ezGALQuery because it is already running.");

  BeginQueryPlatform(query);
}

void ezGALCommandEncoder::EndQuery(ezGALQueryHandle hQuery)
{
  AssertRenderingThread();

  auto query = m_Device.GetQuery(hQuery);
  EZ_ASSERT_DEV(query->m_bStarted, "Can't end ezGALQuery, query hasn't started yet.");

  EndQueryPlatform(query);
}

ezResult ezGALCommandEncoder::GetQueryResult(ezGALQueryHandle hQuery, ezUInt64& uiQueryResult)
{
  AssertRenderingThread();

  auto query = m_Device.GetQuery(hQuery);
  EZ_ASSERT_DEV(!query->m_bStarted, "Can't retrieve data from ezGALQuery while query is still running.");

  return GetQueryResultPlatform(query, uiQueryResult);
}

ezGALTimestampHandle ezGALCommandEncoder::InsertTimestamp()
{
  ezGALTimestampHandle hTimestamp = m_Device.GetTimestamp();

  InsertTimestampPlatform(hTimestamp);

  return hTimestamp;
}

void ezGALCommandEncoder::ClearUnorderedAccessView(ezGALUnorderedAccessViewHandle hUnorderedAccessView, ezVec4 clearValues)
{
  AssertRenderingThread();

  const ezGALUnorderedAccessView* pUnorderedAccessView = m_Device.GetUnorderedAccessView(hUnorderedAccessView);
  if (pUnorderedAccessView == nullptr)
  {
    EZ_REPORT_FAILURE("ClearUnorderedAccessView failed, unordered access view handle invalid.");
    return;
  }

  ClearUnorderedAccessViewPlatform(pUnorderedAccessView, clearValues);
}

void ezGALCommandEncoder::ClearUnorderedAccessView(ezGALUnorderedAccessViewHandle hUnorderedAccessView, ezVec4U32 clearValues)
{
  AssertRenderingThread();

  const ezGALUnorderedAccessView* pUnorderedAccessView = m_Device.GetUnorderedAccessView(hUnorderedAccessView);
  if (pUnorderedAccessView == nullptr)
  {
    EZ_REPORT_FAILURE("ClearUnorderedAccessView failed, unordered access view handle invalid.");
    return;
  }

  ClearUnorderedAccessViewPlatform(pUnorderedAccessView, clearValues);
}

void ezGALCommandEncoder::CopyBuffer(ezGALBufferHandle hDest, ezGALBufferHandle hSource)
{
  AssertRenderingThread();

  const ezGALBuffer* pDest = m_Device.GetBuffer(hDest);
  const ezGALBuffer* pSource = m_Device.GetBuffer(hSource);

  if (pDest != nullptr && pSource != nullptr)
  {
    CopyBufferPlatform(pDest, pSource);
  }
  else
  {
    EZ_REPORT_FAILURE("CopyBuffer failed, buffer handle invalid - destination = {0}, source = {1}", ezArgP(pDest), ezArgP(pSource));
  }
}

void ezGALCommandEncoder::CopyBufferRegion(
  ezGALBufferHandle hDest, ezUInt32 uiDestOffset, ezGALBufferHandle hSource, ezUInt32 uiSourceOffset, ezUInt32 uiByteCount)
{
  AssertRenderingThread();

  const ezGALBuffer* pDest = m_Device.GetBuffer(hDest);
  const ezGALBuffer* pSource = m_Device.GetBuffer(hSource);

  if (pDest != nullptr && pSource != nullptr)
  {
    const ezUInt32 uiDestSize = pDest->GetSize();
    const ezUInt32 uiSourceSize = pSource->GetSize();

    EZ_ASSERT_DEV(uiDestSize >= uiDestOffset + uiByteCount, "Destination buffer too small (or offset too big)");
    EZ_ASSERT_DEV(uiSourceSize >= uiSourceOffset + uiByteCount, "Source buffer too small (or offset too big)");

    CopyBufferRegionPlatform(pDest, uiDestOffset, pSource, uiSourceOffset, uiByteCount);
  }
  else
  {
    EZ_REPORT_FAILURE("CopyBuffer failed, buffer handle invalid - destination = {0}, source = {1}", ezArgP(pDest), ezArgP(pSource));
  }
}

void ezGALCommandEncoder::UpdateBuffer(
  ezGALBufferHandle hDest, ezUInt32 uiDestOffset, ezArrayPtr<const ezUInt8> pSourceData, ezGALUpdateMode::Enum updateMode)
{
  AssertRenderingThread();

  EZ_VERIFY(!pSourceData.IsEmpty(), "Source data for buffer update is invalid!");

  const ezGALBuffer* pDest = m_Device.GetBuffer(hDest);

  if (pDest != nullptr)
  {
    if (updateMode == ezGALUpdateMode::NoOverwrite && !(GetDevice().GetCapabilities().m_bNoOverwriteBufferUpdate))
    {
      updateMode = ezGALUpdateMode::CopyToTempStorage;
    }

    EZ_VERIFY(pDest->GetSize() >= (uiDestOffset + pSourceData.GetCount()), "Buffer is too small (or offset too big)");
    UpdateBufferPlatform(pDest, uiDestOffset, pSourceData, updateMode);
  }
  else
  {
    EZ_REPORT_FAILURE("UpdateBuffer failed, buffer handle invalid");
  }
}

void ezGALCommandEncoder::CopyTexture(ezGALTextureHandle hDest, ezGALTextureHandle hSource)
{
  AssertRenderingThread();

  const ezGALTexture* pDest = m_Device.GetTexture(hDest);
  const ezGALTexture* pSource = m_Device.GetTexture(hSource);

  if (pDest != nullptr && pSource != nullptr)
  {
    CopyTexturePlatform(pDest, pSource);
  }
  else
  {
    EZ_REPORT_FAILURE("CopyTexture failed, texture handle invalid - destination = {0}, source = {1}", ezArgP(pDest), ezArgP(pSource));
  }
}

void ezGALCommandEncoder::CopyTextureRegion(ezGALTextureHandle hDest, const ezGALTextureSubresource& DestinationSubResource,
  const ezVec3U32& DestinationPoint, ezGALTextureHandle hSource, const ezGALTextureSubresource& SourceSubResource, const ezBoundingBoxu32& Box)
{
  AssertRenderingThread();

  const ezGALTexture* pDest = m_Device.GetTexture(hDest);
  const ezGALTexture* pSource = m_Device.GetTexture(hSource);

  if (pDest != nullptr && pSource != nullptr)
  {
    CopyTextureRegionPlatform(pDest, DestinationSubResource, DestinationPoint, pSource, SourceSubResource, Box);
  }
  else
  {
    EZ_REPORT_FAILURE("CopyTextureRegion failed, texture handle invalid - destination = {0}, source = {1}", ezArgP(pDest), ezArgP(pSource));
  }
}

void ezGALCommandEncoder::UpdateTexture(ezGALTextureHandle hDest, const ezGALTextureSubresource& DestinationSubResource,
  const ezBoundingBoxu32& DestinationBox, const ezGALSystemMemoryDescription& pSourceData)
{
  AssertRenderingThread();

  const ezGALTexture* pDest = m_Device.GetTexture(hDest);

  if (pDest != nullptr)
  {
    UpdateTexturePlatform(pDest, DestinationSubResource, DestinationBox, pSourceData);
  }
  else
  {
    EZ_REPORT_FAILURE("UpdateTexture failed, texture handle invalid - destination = {0}", ezArgP(pDest));
  }
}

void ezGALCommandEncoder::ResolveTexture(ezGALTextureHandle hDest, const ezGALTextureSubresource& DestinationSubResource, ezGALTextureHandle hSource,
  const ezGALTextureSubresource& SourceSubResource)
{
  AssertRenderingThread();

  const ezGALTexture* pDest = m_Device.GetTexture(hDest);
  const ezGALTexture* pSource = m_Device.GetTexture(hSource);

  if (pDest != nullptr && pSource != nullptr)
  {
    ResolveTexturePlatform(pDest, DestinationSubResource, pSource, SourceSubResource);
  }
  else
  {
    EZ_REPORT_FAILURE("ResolveTexture failed, texture handle invalid - destination = {0}, source = {1}", ezArgP(pDest), ezArgP(pSource));
  }
}

void ezGALCommandEncoder::ReadbackTexture(ezGALTextureHandle hTexture)
{
  AssertRenderingThread();

  const ezGALTexture* pTexture = m_Device.GetTexture(hTexture);

  if (pTexture != nullptr)
  {
    EZ_ASSERT_RELEASE(pTexture->GetDescription().m_ResourceAccess.m_bReadBack,
      "A texture supplied to read-back needs to be created with the correct resource usage (m_bReadBack = true)!");

    ReadbackTexturePlatform(pTexture);
  }
}

void ezGALCommandEncoder::CopyTextureReadbackResult(ezGALTextureHandle hTexture, const ezArrayPtr<ezGALSystemMemoryDescription>* pData)
{
  AssertRenderingThread();

  const ezGALTexture* pTexture = m_Device.GetTexture(hTexture);

  if (pTexture != nullptr)
  {
    EZ_ASSERT_RELEASE(pTexture->GetDescription().m_ResourceAccess.m_bReadBack,
      "A texture supplied to read-back needs to be created with the correct resource usage (m_bReadBack = true)!");

    CopyTextureReadbackResultPlatform(pTexture, pData);
  }
}

void ezGALCommandEncoder::GenerateMipMaps(ezGALResourceViewHandle hResourceView)
{
  AssertRenderingThread();

  const ezGALResourceView* pResourceView = m_Device.GetResourceView(hResourceView);
  if (pResourceView != nullptr)
  {
    EZ_ASSERT_DEV(!pResourceView->GetDescription().m_hTexture.IsInvalidated(), "Resource view needs a valid texture to generate mip maps.");
    const ezGALTexture* pTexture = m_Device.GetTexture(pResourceView->GetDescription().m_hTexture);
    EZ_ASSERT_DEV(pTexture->GetDescription().m_bAllowDynamicMipGeneration,
      "Dynamic mip map generation needs to be enabled (m_bAllowDynamicMipGeneration = true)!");

    GenerateMipMapsPlatform(pResourceView);
  }
}

void ezGALCommandEncoder::Flush()
{
  AssertRenderingThread();

  FlushPlatform();
}

// Debug helper functions

void ezGALCommandEncoder::PushMarker(const char* Marker)
{
  AssertRenderingThread();

  EZ_ASSERT_DEV(Marker != nullptr, "Invalid marker!");

  PushMarkerPlatform(Marker);
}

void ezGALCommandEncoder::PopMarker()
{
  AssertRenderingThread();

  PopMarkerPlatform();
}

void ezGALCommandEncoder::InsertEventMarker(const char* Marker)
{
  AssertRenderingThread();

  EZ_ASSERT_DEV(Marker != nullptr, "Invalid marker!");

  InsertEventMarkerPlatform(Marker);
}

void ezGALCommandEncoder::ClearStatisticsCounters()
{
  // Reset counters for various statistics
  m_uiStateChanges = 0;
  m_uiRedundantStateChanges = 0;
}

void ezGALCommandEncoder::InvalidateState()
{
  m_hShader = ezGALShaderHandle();

  EZ_ASSERT_NOT_IMPLEMENTED;
  //m_hConstantBuffers = {};


  // ezGALBufferHandle m_hConstantBuffers[EZ_GAL_MAX_CONSTANT_BUFFER_COUNT];

  // ezGALResourceViewHandle m_hResourceViews[ezGALShaderStage::ENUM_COUNT][EZ_GAL_MAX_SHADER_RESOURCE_VIEW_COUNT];

  // ezGALSamplerStateHandle m_hSamplerStates[ezGALShaderStage::ENUM_COUNT][EZ_GAL_MAX_SHADER_RESOURCE_VIEW_COUNT];
}

bool ezGALCommandEncoder::UnsetResourceViews(const ezGALResourceBase* pResource)
{
  EZ_ASSERT_DEV(pResource->GetParentResource() == pResource, "No proxies allowed");

  bool bResult = false;

  for (ezUInt32 stage = 0; stage < ezGALShaderStage::ENUM_COUNT; ++stage)
  {
    for (ezUInt32 uiSlot = 0; uiSlot < m_pResourcesForResourceViews[stage].GetCount(); ++uiSlot)
    {
      if (m_pResourcesForResourceViews[stage][uiSlot] == pResource)
      {
        SetResourceViewPlatform((ezGALShaderStage::Enum)stage, uiSlot, nullptr);

        m_hResourceViews[stage][uiSlot].Invalidate();
        m_pResourcesForResourceViews[stage][uiSlot] = nullptr;

        bResult = true;
      }
    }
  }

  return bResult;
}

bool ezGALCommandEncoder::UnsetUnorderedAccessViews(const ezGALResourceBase* pResource)
{
  EZ_ASSERT_DEV(pResource->GetParentResource() == pResource, "No proxies allowed");

  bool bResult = false;

  for (ezUInt32 uiSlot = 0; uiSlot < m_pResourcesForUnorderedAccessViews.GetCount(); ++uiSlot)
  {
    if (m_pResourcesForUnorderedAccessViews[uiSlot] == pResource)
    {
      SetUnorderedAccessViewPlatform(uiSlot, nullptr);

      m_hUnorderedAccessViews[uiSlot].Invalidate();
      m_pResourcesForUnorderedAccessViews[uiSlot] = nullptr;

      bResult = true;
    }
  }

  return bResult;
}