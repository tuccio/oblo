#pragma once

namespace oblo
{
    template <typename T>
    struct intrusive_list
    {
        T* head = nullptr;

        void push_front(T* node) noexcept
        {
            node->intrusiveNext = head;
            node->intrusivePrev = nullptr;

            if (head)
            {
                head->intrusivePrev = node;
            }

            head = node;
        }

        void remove(T* node) noexcept
        {
            if (node->intrusivePrev)
            {
                node->intrusivePrev->intrusiveNext = node->intrusiveNext;
            }
            else
            {
                head = node->intrusiveNext;
            }

            if (node->intrusiveNext)
            {
                node->intrusiveNext->intrusivePrev = node->intrusivePrev;
            }

            node->intrusiveNext = nullptr;
            node->intrusivePrev = nullptr;
        }

        bool empty() const noexcept
        {
            return head == nullptr;
        }
    };
}