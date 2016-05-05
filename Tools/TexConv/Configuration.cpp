#include "Main.h"

ezTexConv::ezTexConv()
{
  m_TextureType = TextureType::Texture2D;
  m_bGeneratedMipmaps = false;
  m_bCompress = false;
  m_uiOutputChannels = 4;
  m_bSRGBOutput = false;
  m_uiAssetHash = 0;
  m_uiAssetVersion = 0;
}

void ezTexConv::AfterCoreStartup()
{
  ezGlobalLog::AddLogWriter(ezLogWriter::Console::LogMessageHandler);
  ezGlobalLog::AddLogWriter(ezLogWriter::VisualStudio::LogMessageHandler);

  // Add standard folder factory
  ezFileSystem::RegisterDataDirectoryFactory(ezDataDirectory::FolderType::Factory);

  // Add the empty data directory to access files via absolute paths
  ezFileSystem::AddDataDirectory("");

  m_2dSource[0].m_uiChannelMask = EZ_BIT(0);
  m_2dSource[1].m_uiChannelMask = EZ_BIT(1);
  m_2dSource[2].m_uiChannelMask = EZ_BIT(2);
  m_2dSource[3].m_uiChannelMask = EZ_BIT(3);

  ParseCommandLine();
}

void ezTexConv::BeforeCoreShutdown()
{
  for (auto pImg : m_CleanupImages)
  {
    EZ_DEFAULT_DELETE(pImg);
  }

  m_CleanupImages.Clear();

  ezGlobalLog::RemoveLogWriter(ezLogWriter::Console::LogMessageHandler);
  ezGlobalLog::RemoveLogWriter(ezLogWriter::VisualStudio::LogMessageHandler);
}

ezTexConv::ChannelMapping ezTexConv::ParseInputCfg(const char* cfg, ezInt8 iChannelIndex, bool bSingleChannel)
{
  ChannelMapping source;
  source.m_iInput = -1;
  source.m_uiChannelMask = 0;

  ezStringBuilder tmp = cfg;

  // '-r black' for setting it to zero
  if (tmp.IsEqual_NoCase("black"))
    return source;

  // 'r white' for setting it to 255
  if (tmp.IsEqual_NoCase("white"))
  {
    source.m_uiChannelMask = Channel::All;
    return source;
  }

  // skip the 'in', if found
  // is optional though, one can also write '-r 1.r' for '-r in1.r'
  if (tmp.StartsWith_NoCase("in"))
    tmp.Shrink(2, 0);

  if (tmp.StartsWith("."))
  {
    // no index given, e.g. '-r in.r'
    // in is equal to in0

    source.m_iInput = 0;
  }
  else
  {
    ezInt32 num = -1;
    const char* szLastPos = nullptr;
    if (ezConversionUtils::StringToInt(tmp, num, &szLastPos).Failed())
    {
      ezLog::Error("Could not parse input config '%s'", cfg);
      return source;
    }

    // valid index after the 'in'
    if (num >= 0 && num < (ezInt32)m_InputFileNames.GetCount())
    {
      source.m_iInput = num;
    }
    else
    {
      ezLog::Error("Invalid Input index '%u'", num);
      return source;
    }

    ezStringBuilder dummy = szLastPos;

    // continue after the index
    tmp = dummy;
  }

  // no additional info, e.g. '-g in2' is identical to '-g in2.g' (same channel)
  if (tmp.IsEmpty())
  {
    source.m_uiChannelMask = EZ_BIT(iChannelIndex);
    return source;
  }

  if (!tmp.StartsWith("."))
  {
    ezLog::Error("Expected '.' after input index in '%s'", cfg);
    return source;
  }

  tmp.Shrink(1, 0);

  if (!bSingleChannel)
  {
    // in case of '-rgb in1.bgr' map r to b, g to g, b to r, etc.
    // in case of '-rgb in1.r' map everything to the same input
    if (tmp.GetCharacterCount() > 1)
      tmp.Shrink(iChannelIndex, 0);
  }

  // no additional info, e.g. '-rgb in2.rg' will map b to b
  if (tmp.IsEmpty())
  {
    ezLog::Error("Bad input config '%s'", cfg);

    source.m_uiChannelMask = EZ_BIT(iChannelIndex);
    return source;
  }

  // for multi-channel assignments do this exactly once
  // for single-channel assignments, find all channels that should be merged into it
  do
  {
    if (tmp.StartsWith_NoCase("r"))
      source.m_uiChannelMask |= Channel::Red;

    if (tmp.StartsWith_NoCase("g"))
      source.m_uiChannelMask |= Channel::Green;

    if (tmp.StartsWith_NoCase("b"))
      source.m_uiChannelMask |= Channel::Blue;

    if (tmp.StartsWith_NoCase("a"))
      source.m_uiChannelMask |= Channel::Alpha;

    tmp.Shrink(1, 0);
  }
  while (bSingleChannel && !tmp.IsEmpty());

  return source;
}

void ezTexConv::ParseCommandLine()
{
  auto pCmd = ezCommandLineUtils::GetGlobalInstance();

  ezStringBuilder res, tmp;

  for (ezUInt32 i = 0; i < 32; ++i)
  {
    tmp.Format("-in%u", i);

    res = pCmd->GetStringOption(tmp);

    // stop once one option was not found
    if (res.IsEmpty())
      break;

    m_InputFileNames.PushBack(res);
  }

  if (m_InputFileNames.IsEmpty()) // try '-in' in this case, ignore it otherwise
  {
    // short version for -in1
    res = pCmd->GetStringOption("-in");

    if (!res.IsEmpty())
      m_InputFileNames.PushBack(res);
  }

  // target file
  {
    m_sOutputFile = pCmd->GetStringOption("-out");
  }

  // some boolean states
  {
    m_bGeneratedMipmaps = pCmd->GetBoolOption("-mipmaps", false);
    m_bCompress = pCmd->GetBoolOption("-compress", false);
    m_bSRGBOutput = pCmd->GetBoolOption("-srgb", false);
    m_uiOutputChannels = pCmd->GetIntOption("-channels", 4);
  }

  // Texture Type
  {
    if (pCmd->GetBoolOption("-cubemap"))
      m_TextureType = TextureType::Cubemap;
  }

  // input to output mappings
  {
    tmp = pCmd->GetStringOption("-rgba");
    if (!tmp.IsEmpty())
    {
      m_2dSource[0] = ParseInputCfg(tmp, 0, false);
      m_2dSource[1] = ParseInputCfg(tmp, 1, false);
      m_2dSource[2] = ParseInputCfg(tmp, 2, false);
      m_2dSource[3] = ParseInputCfg(tmp, 3, false);
    }

    tmp = pCmd->GetStringOption("-rgb");
    if (!tmp.IsEmpty())
    {
      m_2dSource[0] = ParseInputCfg(tmp, 0, false);
      m_2dSource[1] = ParseInputCfg(tmp, 1, false);
      m_2dSource[2] = ParseInputCfg(tmp, 2, false);
    }

    tmp = pCmd->GetStringOption("-rg");
    if (!tmp.IsEmpty())
    {
      m_2dSource[0] = ParseInputCfg(tmp, 0, false);
      m_2dSource[1] = ParseInputCfg(tmp, 1, false);
    }

    tmp = pCmd->GetStringOption("-r");
    if (!tmp.IsEmpty())
      m_2dSource[0] = ParseInputCfg(tmp, 0, true);

    tmp = pCmd->GetStringOption("-g");
    if (!tmp.IsEmpty())
      m_2dSource[1] = ParseInputCfg(tmp, 1, true);

    tmp = pCmd->GetStringOption("-b");
    if (!tmp.IsEmpty())
      m_2dSource[2] = ParseInputCfg(tmp, 2, true);

    tmp = pCmd->GetStringOption("-a");
    if (!tmp.IsEmpty())
      m_2dSource[3] = ParseInputCfg(tmp, 3, true);
  }

  // Asset Hash and Version
  {
    m_uiAssetVersion = pCmd->GetIntOption("-assetVersion", 0);

    const ezString sHashLow = pCmd->GetStringOption("-assetHashLow");
    ezUInt64 uiHashLow = ezConversionUtils::ConvertHexStringToUInt32(sHashLow);
    const ezString sHashHigh = pCmd->GetStringOption("-assetHashHigh");
    ezUInt64 uiHashHigh = ezConversionUtils::ConvertHexStringToUInt32(sHashHigh);

    m_uiAssetHash = (uiHashHigh << 32) | uiHashLow;
  }
}

ezResult ezTexConv::ValidateConfiguration()
{
  if (m_InputFileNames.IsEmpty())
  {
    ezLog::Error("No input files are specified. Use the -in command for a single file, or -in0 ... -in31 for multiple input files");
    return EZ_FAILURE;
  }

  if (m_sOutputFile.IsEmpty())
  {
    ezLog::Error("No output file is specified. Use the -out command for this");
    return EZ_FAILURE;
  }

  if (m_uiOutputChannels > 4)
  {
    ezLog::Error("Number of target channels (%u) is invalid", m_uiOutputChannels);
    return EZ_FAILURE;
  }

  if (m_uiOutputChannels < 3 && m_bSRGBOutput)
  {
    ezLog::Error("-srgb flag is invalid for 1 and 2 channel textures, as they do not support sRGB output.");
    return EZ_FAILURE;
  }

  return EZ_SUCCESS;
}

void ezTexConv::PrintConfig()
{
  EZ_LOG_BLOCK("Configuration");

  ezLog::Info("Output: '%s'", m_sOutputFile.GetData());

  for (ezUInt32 i = 0; i < m_InputFileNames.GetCount(); ++i)
  {
    ezLog::Info("Input %u: '%s'", i, m_InputFileNames[i].GetData());
  }

  ezLog::Info("Generate Mipmaps: %s", m_bGeneratedMipmaps ? "yes" : "no");
  ezLog::Info("Use Compression: %s", m_bCompress ? "yes" : "no");
  ezLog::Info("Output Channels: %u", m_uiOutputChannels);
  ezLog::Info("Output is %s", m_bSRGBOutput ? "sRGB" : "Linear");

  for (ezUInt32 i = 0; i < m_uiOutputChannels; ++i)
  {
    if (m_2dSource[i].m_iInput == -1)
    {
      ezLog::Info("Output[%u] = %s", i, m_2dSource[i].m_uiChannelMask == 0 ? "black" : "white");
    }
    else
    {
      ezLog::Info("Output[%u] = Input[%i].%s", i, m_2dSource[i].m_iInput, ChannelMaskToString(m_2dSource[i].m_uiChannelMask).GetData());
    }
  }

  switch (m_TextureType)
  {
  case TextureType::Texture2D:
    ezLog::Info("Type: 2D Texture");
    break;
  case TextureType::Cubemap:
    ezLog::Info("Type: Cubemap");
    break;
  default:
    EZ_ASSERT_NOT_IMPLEMENTED;
  }
}
