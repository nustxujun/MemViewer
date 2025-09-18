#pragma once 

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "PlatformCommons.h"
#include "HAL/PlatformTLS.h"

template<class T>
class FDynamicTLS
{
public:
    FDynamicTLS()
    {
        Slot = FPlatformTLS::AllocTlsSlot();
    }

    ~FDynamicTLS()
    {
        FPlatformTLS::FreeTlsSlot(Slot);

        while(!Buffers.IsEmpty())
        {
            void* Buffer;
            if (Buffers.Dequeue(Buffer))
            {
                FGenericPlatformInternalAllocator::Free(Buffer);
            }
        }
    }

    void* GetData()
    {
        auto Buffer = FPlatformTLS::GetTlsValue(Slot);
        if (Buffer == nullptr)
        {
            Buffer = FGenericPlatformInternalAllocator::Malloc(sizeof(T));
            memset(Buffer, 0 , sizeof(T));
            FPlatformTLS::SetTlsValue(Slot, Buffer);
            Buffers.Enqueue(Buffer);
        }

        return Buffer;
    }

    T GetValue()
    {
        return *(T*)GetData();
    }

    void SetValue(T Value)
    {
        auto Val = (T*)GetData();
        *Val = MoveTemp(Value);
    }

private:
    TQueue<void*, EQueueMode::Mpsc> Buffers;
    uint32 Slot;
};