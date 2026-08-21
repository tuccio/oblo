#pragma once

namespace oblo
{
    template <typename Iterator>
    bool equal(Iterator lhsBegin, const Iterator& lhsEnd, Iterator rhsBegin)
    {
        while (lhsBegin != lhsEnd)
        {
            if (!(*lhsBegin == *rhsBegin))
            {
                return false;
            }

            ++lhsBegin;
            ++rhsBegin;
        }

        return true;
    }

    template <typename Iterator>
    bool equal(Iterator lhsBegin, const Iterator& lhsEnd, Iterator rhsBegin, const Iterator& rhsEnd)
    {
        while (lhsBegin != lhsEnd && rhsBegin != rhsEnd)
        {
            if (!(*lhsBegin == *rhsBegin))
            {
                return false;
            }

            ++lhsBegin;
            ++rhsBegin;
        }

        return lhsBegin == lhsEnd && rhsBegin == rhsEnd;
    }
}