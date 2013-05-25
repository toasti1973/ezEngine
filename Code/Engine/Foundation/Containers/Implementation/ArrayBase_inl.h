
template <typename T, typename Derived>
ezArrayBase<T, Derived>::ezArrayBase()
{
  m_pElements = NULL;
  m_uiCount = 0;
}

template <typename T, typename Derived>
ezArrayBase<T, Derived>::~ezArrayBase()
{
  EZ_ASSERT(m_uiCount == 0, "The derived class did not destruct all objects. Count is %i.", m_uiCount);
  EZ_ASSERT(m_pElements == NULL, "The derived class did not free its memory.");
}

template <typename T, typename Derived>
EZ_FORCE_INLINE ezArrayBase<T, Derived>::operator const ezArrayPtr<T>() const
{
  return ezArrayPtr<T>(m_pElements, m_uiCount);
}

template <typename T, typename Derived>
EZ_FORCE_INLINE ezArrayBase<T, Derived>::operator ezArrayPtr<T>()
{
  return ezArrayPtr<T>(m_pElements, m_uiCount);
}

template <typename T, typename Derived>
bool ezArrayBase<T, Derived>::operator== (const ezArrayPtr<T>& rhs) const
{
  if (m_uiCount != rhs.GetCount())
    return false;

  return ezMemoryUtils::IsEqual(m_pElements, rhs.GetPtr(), m_uiCount);
}

template <typename T, typename Derived>
EZ_FORCE_INLINE bool ezArrayBase<T, Derived>::operator!= (const ezArrayPtr<T>& rhs) const
{
  return !(*this == rhs);
}

template <typename T, typename Derived>
EZ_FORCE_INLINE const T& ezArrayBase<T, Derived>::operator[](const ezUInt32 uiIndex) const
{
  EZ_ASSERT(uiIndex < m_uiCount, "Out of bounds access. Array has %i elements, trying to access element at index %i.", m_uiCount, uiIndex);
  return m_pElements[uiIndex];
}

template <typename T, typename Derived>
EZ_FORCE_INLINE T& ezArrayBase<T, Derived>::operator[](const ezUInt32 uiIndex)
{
  EZ_ASSERT(uiIndex < m_uiCount, "Out of bounds access. Array has %i elements, trying to access element at index %i.", m_uiCount, uiIndex);
  return m_pElements[uiIndex];
}

template <typename T, typename Derived>
EZ_FORCE_INLINE ezUInt32 ezArrayBase<T, Derived>::GetCount() const
{
  return m_uiCount;
}

template <typename T, typename Derived>
EZ_FORCE_INLINE bool ezArrayBase<T, Derived>::IsEmpty() const
{
  return m_uiCount == 0;
}

template <typename T, typename Derived>
void ezArrayBase<T, Derived>::Clear()
{
  ezMemoryUtils::Destruct(m_pElements, m_uiCount);
  m_uiCount = 0;
}

template <typename T, typename Derived>
bool ezArrayBase<T, Derived>::Contains(const T& value) const
{
  return IndexOf(value) != ezInvalidIndex;
}

template <typename T, typename Derived>
void ezArrayBase<T, Derived>::PushBackUnchecked(const T& value)
{
  EZ_ASSERT(m_uiCount < m_uiCapacity, "Appending unchecked to array with insufficient capacity.");

  ezMemoryUtils::Construct(m_pElements + m_uiCount, value, 1);
  m_uiCount++;
}

template <typename T, typename Derived>
void ezArrayBase<T, Derived>::PushBackRange(const ezArrayPtr<T>& range)
{
  const ezUInt32 uiRangeCount = range.GetCount();
  static_cast<Derived*>(this)->Reserve(m_uiCount + uiRangeCount);
  
  ezMemoryUtils::Construct(m_pElements + m_uiCount, range.GetPtr(), uiRangeCount);
  m_uiCount += uiRangeCount;
}

template <typename T, typename Derived>
void ezArrayBase<T, Derived>::PushBackRange(const ezArrayPtr<const T>& range)
{
  const ezUInt32 uiRangeCount = range.GetCount();
  static_cast<Derived*>(this)->Reserve(m_uiCount + uiRangeCount);
  
  ezMemoryUtils::Construct(m_pElements + m_uiCount, range.GetPtr(), uiRangeCount);
  m_uiCount += uiRangeCount;
}

template <typename T, typename Derived>
void ezArrayBase<T, Derived>::Insert(const T& value, ezUInt32 uiIndex)
{
  EZ_ASSERT(uiIndex <= m_uiCount, "Invalid index. Array has %i elements, trying to insert element at index %i.", m_uiCount, uiIndex);
  
  static_cast<Derived*>(this)->Reserve(m_uiCount + 1);

  ezMemoryUtils::Construct(m_pElements + m_uiCount, 1);
  ezMemoryUtils::Move(m_pElements + uiIndex + 1, m_pElements + uiIndex, m_uiCount - uiIndex);
  m_pElements[uiIndex] = value;
  m_uiCount++;
}

template <typename T, typename Derived>
bool ezArrayBase<T, Derived>::Remove(const T& value)
{
  ezUInt32 uiIndex = IndexOf(value);

  if (uiIndex == ezInvalidIndex)
    return false;

  RemoveAt(uiIndex);
  return true;
}

template <typename T, typename Derived>
bool ezArrayBase<T, Derived>::RemoveSwap(const T& value)
{
  ezUInt32 uiIndex = IndexOf(value);

  if (uiIndex == ezInvalidIndex)
    return false;

  RemoveAtSwap(uiIndex);
  return true;
}

template <typename T, typename Derived>
void ezArrayBase<T, Derived>::RemoveAt(ezUInt32 uiIndex)
{
  EZ_ASSERT(uiIndex < m_uiCount, "Out of bounds access. Array has %i elements, trying to remove element at index %i.", m_uiCount, uiIndex);

  m_uiCount--;
  ezMemoryUtils::Move(m_pElements + uiIndex, m_pElements + uiIndex + 1, m_uiCount - uiIndex); 
  ezMemoryUtils::Destruct(m_pElements + m_uiCount, 1);
}

template <typename T, typename Derived>
void ezArrayBase<T, Derived>::RemoveAtSwap(ezUInt32 uiIndex)
{
  EZ_ASSERT(uiIndex < m_uiCount, "Out of bounds access. Array has %i elements, trying to remove element at index %i.", m_uiCount, uiIndex);
  
  m_uiCount--;
  if (m_uiCount != uiIndex)
  {
    m_pElements[uiIndex] = m_pElements[m_uiCount];
  }
  ezMemoryUtils::Destruct(m_pElements + m_uiCount, 1);
}

template <typename T, typename Derived>
ezUInt32 ezArrayBase<T, Derived>::IndexOf(const T& value, ezUInt32 uiStartIndex) const
{
  for (ezUInt32 i = uiStartIndex; i < m_uiCount; i++)
  {
    if (ezMemoryUtils::IsEqual(m_pElements + i, &value))
      return i;
  }
  return ezInvalidIndex;
}

template <typename T, typename Derived>
ezUInt32 ezArrayBase<T, Derived>::LastIndexOf(const T& value, ezUInt32 uiStartIndex) const
{
  for (ezUInt32 i = ezMath::Min(uiStartIndex, m_uiCount); i-- > 0; )
  {
    if (ezMemoryUtils::IsEqual(m_pElements + i, &value))
      return i;
  }
  return ezInvalidIndex;
}

template <typename T, typename Derived>
void ezArrayBase<T, Derived>::PushBack(const T& value)
{
  static_cast<Derived*>(this)->Reserve(m_uiCount + 1);
  
  ezMemoryUtils::Construct(m_pElements + m_uiCount, value, 1);
  m_uiCount++;
}

template <typename T, typename Derived>
void ezArrayBase<T, Derived>::PopBack(ezUInt32 uiCountToRemove /* = 1 */)
{
  EZ_ASSERT(m_uiCount >= uiCountToRemove, "Out of bounds access. Array has %i elements, trying to pop %i elements.", m_uiCount, uiCountToRemove);

  m_uiCount -= uiCountToRemove;
  ezMemoryUtils::Destruct(m_pElements + m_uiCount, uiCountToRemove);
}

template <typename T, typename Derived>
EZ_FORCE_INLINE T& ezArrayBase<T, Derived>::PeekBack()
{
  EZ_ASSERT(m_uiCount > 0, "Out of bounds access. Trying to peek into an empty array.");
  return m_pElements[m_uiCount - 1];
}

template <typename T, typename Derived>
EZ_FORCE_INLINE const T& ezArrayBase<T, Derived>::PeekBack() const
{
  EZ_ASSERT(m_uiCount > 0, "Out of bounds access. Trying to peek into an empty array.");
  return m_pElements[m_uiCount - 1];
}

template <typename T, typename Derived>
template <typename C>
void ezArrayBase<T, Derived>::Sort()
{
  if (m_uiCount > 1)
    ezSorting<C>::QuickSort((ezArrayPtr<T>)*this);
}

template <typename T, typename Derived>
void ezArrayBase<T, Derived>::Sort()
{
  if (m_uiCount > 1)
    ezSorting<ezCompareHelper<T> >::QuickSort((ezArrayPtr<T>&)*this);
}
