#pragma once 

#if PLATFORM_WINDOWS
#include "Platforms/Windows/WinHook.h"
#include "Platforms/Windows/WindowsInternalAllocator.h"
#include "Platforms/Windows/WindowsMemoryInfo.h"
#include "Platforms/Windows/WindowsExternalTracker.h"
#elif PLATFORM_IOS
#include "Platforms/IOS/IOSHook.h"
#include "Platforms/IOS/IOSInternalAllocator.h"
#include "Platforms/IOS/IOSMemoryInfo.h"
#elif PLATFORM_ANDROID
#include "Platforms/Android/AndroidHook.h"
#include "Platforms/Android/AndroidInternalAllocator.h"
#include "Platforms/Android/AndroidMemoryInfo.h"
#include "Platforms/Android/AndroidUnwind.h"
#endif


#if ENGINE_MAJOR_VERSION == 5
template <int IndexSize>
class TInternalSizedHeapAllocator
{
public:
	using SizeType = typename TBitsToSizeType<IndexSize>::Type;

private:
	using USizeType = std::make_unsigned_t<SizeType>;

public:
	enum { NeedsElementType = true };
	enum { RequireRangeCheck = true };

	class ForAnyElementType
	{
		template <int>
		friend class TInternalSizedHeapAllocator;

	public:
		ForAnyElementType()
			: Data(nullptr)
		{
		}

		template <typename OtherAllocator>
		FORCEINLINE void MoveToEmptyFromOtherAllocator(typename OtherAllocator::ForAnyElementType& Other)
		{
			checkSlow((void*)this != (void*)&Other);

			if (Data)
			{
				FGenericPlatformInternalAllocator::Free(Data);
			}

			Data = Other.Data;
			Other.Data = nullptr;
		}

		FORCEINLINE void MoveToEmpty(ForAnyElementType& Other)
		{
			this->MoveToEmptyFromOtherAllocator<TInternalSizedHeapAllocator>(Other);
		}

		FORCEINLINE ~ForAnyElementType()
		{
			if (Data)
			{
				FGenericPlatformInternalAllocator::Free(Data);
			}
		}

		FORCEINLINE FScriptContainerElement* GetAllocation() const
		{
			return Data;
		}
		void ResizeAllocation(SizeType PreviousNumElements, SizeType NumElements, SIZE_T NumBytesPerElement)
		{
			if (Data || NumElements)
			{
				static_assert(sizeof(SizeType) <= sizeof(SIZE_T), "SIZE_T is expected to handle all possible sizes");

				bool bInvalidResize = NumElements < 0 || NumBytesPerElement < 1 || NumBytesPerElement >(SIZE_T)MAX_int32;
				if constexpr (sizeof(SizeType) == sizeof(SIZE_T))
				{
					bInvalidResize = bInvalidResize || (SIZE_T)(USizeType)NumElements > (SIZE_T)TNumericLimits<SizeType>::Max() / NumBytesPerElement;
				}
				if (UNLIKELY(bInvalidResize))
				{
					UE::Core::Private::OnInvalidSizedHeapAllocatorNum(IndexSize, NumElements, NumBytesPerElement);
				}

				Data = (FScriptContainerElement*)FGenericPlatformInternalAllocator::Realloc(Data, NumElements * NumBytesPerElement);
			}
		}
		void ResizeAllocation(SizeType PreviousNumElements, SizeType NumElements, SIZE_T NumBytesPerElement, uint32 AlignmentOfElement)
		{
			if (Data || NumElements)
			{
				static_assert(sizeof(SizeType) <= sizeof(SIZE_T), "SIZE_T is expected to handle all possible sizes");

				bool bInvalidResize = NumElements < 0 || NumBytesPerElement < 1 || NumBytesPerElement >(SIZE_T)MAX_int32;
				if constexpr (sizeof(SizeType) == sizeof(SIZE_T))
				{
					bInvalidResize = bInvalidResize || ((SIZE_T)(USizeType)NumElements > (SIZE_T)TNumericLimits<SizeType>::Max() / NumBytesPerElement);
				}
				if (UNLIKELY(bInvalidResize))
				{
					UE::Core::Private::OnInvalidSizedHeapAllocatorNum(IndexSize, NumElements, NumBytesPerElement);
				}

				Data = (FScriptContainerElement*)FGenericPlatformInternalAllocator::Realloc(Data, NumElements * NumBytesPerElement);
			}
		}
		FORCEINLINE SizeType CalculateSlackReserve(SizeType NumElements, SIZE_T NumBytesPerElement) const
		{
			return DefaultCalculateSlackReserve(NumElements, NumBytesPerElement, true);
		}
		FORCEINLINE SizeType CalculateSlackReserve(SizeType NumElements, SIZE_T NumBytesPerElement, uint32 AlignmentOfElement) const
		{
			return DefaultCalculateSlackReserve(NumElements, NumBytesPerElement, true, (uint32)AlignmentOfElement);
		}
		FORCEINLINE SizeType CalculateSlackShrink(SizeType NumElements, SizeType NumAllocatedElements, SIZE_T NumBytesPerElement) const
		{
			return DefaultCalculateSlackShrink(NumElements, NumAllocatedElements, NumBytesPerElement, true);
		}
		FORCEINLINE SizeType CalculateSlackShrink(SizeType NumElements, SizeType NumAllocatedElements, SIZE_T NumBytesPerElement, uint32 AlignmentOfElement) const
		{
			return DefaultCalculateSlackShrink(NumElements, NumAllocatedElements, NumBytesPerElement, true, (uint32)AlignmentOfElement);
		}
		FORCEINLINE SizeType CalculateSlackGrow(SizeType NumElements, SizeType NumAllocatedElements, SIZE_T NumBytesPerElement) const
		{
			return DefaultCalculateSlackGrow(NumElements, NumAllocatedElements, NumBytesPerElement, true);
		}
		FORCEINLINE SizeType CalculateSlackGrow(SizeType NumElements, SizeType NumAllocatedElements, SIZE_T NumBytesPerElement, uint32 AlignmentOfElement) const
		{
			return DefaultCalculateSlackGrow(NumElements, NumAllocatedElements, NumBytesPerElement, true, (uint32)AlignmentOfElement);
		}

		SIZE_T GetAllocatedSize(SizeType NumAllocatedElements, SIZE_T NumBytesPerElement) const
		{
			return NumAllocatedElements * NumBytesPerElement;
		}

		bool HasAllocation() const
		{
			return !!Data;
		}

		SizeType GetInitialCapacity() const
		{
			return 0;
		}

	private:
		//ForAnyElementType(const ForAnyElementType&);
		//ForAnyElementType& operator=(const ForAnyElementType&);

		FScriptContainerElement* Data;
	};

	template<typename ElementType>
	class ForElementType : public ForAnyElementType
	{
	public:
		/** Default constructor. */
		ForElementType()
		{
		}

		FORCEINLINE ElementType* GetAllocation() const
		{
			return (ElementType*)ForAnyElementType::GetAllocation();
		}
	};
};

using FDefaultInternalAllocator = TInternalSizedHeapAllocator<32>;

template <uint32 NumInlineElements, typename SecondaryAllocator = FDefaultInternalAllocator>
using TInternalInlineAllocator = TSizedInlineAllocator<NumInlineElements, 32, SecondaryAllocator>;

using FDefaultInternalSparseArrayAllocator = TSparseArrayAllocator< FDefaultInternalAllocator, TInternalInlineAllocator<4>>;
using FDefaultInternalSetAllocator = TSetAllocator< FDefaultInternalSparseArrayAllocator, TInternalInlineAllocator<1>>;


#elif ENGINE_MAJOR_VERSION == 4
	
template <int IndexSize>
class TInternalSizedHeapAllocator
{
public:
	using SizeType = typename TBitsToSizeType<IndexSize>::Type;

	enum { NeedsElementType = false };
	enum { RequireRangeCheck = true };

	class ForAnyElementType
	{
		template <int>
		friend class TSizedHeapAllocator;

	public:
		ForAnyElementType()
			: Data(nullptr)
		{}

		
		template <typename OtherAllocator>
		FORCEINLINE void MoveToEmptyFromOtherAllocator(typename OtherAllocator::ForAnyElementType& Other)
		{
			checkSlow((void*)this != (void*)&Other);

			if (Data)
			{
				FMemory::Free(Data);
			}

			Data = Other.Data;
			Other.Data = nullptr;
		}

		FORCEINLINE void MoveToEmpty(ForAnyElementType& Other)
		{
			this->MoveToEmptyFromOtherAllocator<TSizedHeapAllocator>(Other);
		}

		/** Destructor. */
		FORCEINLINE ~ForAnyElementType()
		{
			if(Data)
			{
				FGenericPlatformInternalAllocator::Free(Data);
			}
		}

		// FContainerAllocatorInterface
		FORCEINLINE FScriptContainerElement* GetAllocation() const
		{
			return Data;
		}
		FORCEINLINE void ResizeAllocation(SizeType PreviousNumElements, SizeType NumElements, SIZE_T NumBytesPerElement)
		{
			// Avoid calling FMemory::Realloc( nullptr, 0 ) as ANSI C mandates returning a valid pointer which is not what we want.
			if (Data || NumElements)
			{
				//checkSlow(((uint64)NumElements*(uint64)ElementTypeInfo.GetSize() < (uint64)INT_MAX));
				Data = (FScriptContainerElement*)FGenericPlatformInternalAllocator::Realloc( Data, NumElements*NumBytesPerElement );
			}
		}
		FORCEINLINE SizeType CalculateSlackReserve(SizeType NumElements, SIZE_T NumBytesPerElement) const
		{
			return DefaultCalculateSlackReserve(NumElements, NumBytesPerElement, true);
		}
		FORCEINLINE SizeType CalculateSlackShrink(SizeType NumElements, SizeType NumAllocatedElements, SIZE_T NumBytesPerElement) const
		{
			return DefaultCalculateSlackShrink(NumElements, NumAllocatedElements, NumBytesPerElement, true);
		}
		FORCEINLINE SizeType CalculateSlackGrow(SizeType NumElements, SizeType NumAllocatedElements, SIZE_T NumBytesPerElement) const
		{
			return DefaultCalculateSlackGrow(NumElements, NumAllocatedElements, NumBytesPerElement, true);
		}

		SIZE_T GetAllocatedSize(SizeType NumAllocatedElements, SIZE_T NumBytesPerElement) const
		{
			return NumAllocatedElements * NumBytesPerElement;
		}

		bool HasAllocation() const
		{
			return !!Data;
		}

		SizeType GetInitialCapacity() const
		{
			return 0;
		}

	private:
//		ForAnyElementType(const ForAnyElementType&);
//		ForAnyElementType& operator=(const ForAnyElementType&);

		/** A pointer to the container's elements. */
		FScriptContainerElement* Data;
	};
	
	template<typename ElementType>
	class ForElementType : public ForAnyElementType
	{
	public:

		/** Default constructor. */
		ForElementType()
		{}

		FORCEINLINE ElementType* GetAllocation() const
		{
			return (ElementType*)ForAnyElementType::GetAllocation();
		}
	};
};
using FDefaultInternalAllocator = TInternalSizedHeapAllocator<32>;


using FDefaultInternalSparseArrayAllocator = TSparseArrayAllocator<FDefaultInternalAllocator, TInlineAllocator<4, FDefaultInternalAllocator>>;
using FDefaultInternalSetAllocator = TSetAllocator< FDefaultInternalSparseArrayAllocator, TInlineAllocator<1,FDefaultInternalAllocator>>;


#endif

