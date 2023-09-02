#include <oblo/resource/resource_handle.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/resource/resource.hpp>

namespace oblo
{
    resource_handle::resource_handle(resource* resource)
    {
        if (resource)
        {
            resource_acquire(resource);
            m_resource = resource;
            m_ptr = resource->data;
        }
    }

    resource_handle::resource_handle(const resource_handle& other)
    {
        m_ptr = other.m_ptr;
        m_resource = other.m_resource;

        if (m_resource)
        {
            resource_acquire(m_resource);
        }
    }

    resource_handle& resource_handle::operator=(const resource_handle& other)
    {
        reset();

        m_ptr = other.m_ptr;
        m_resource = other.m_resource;

        if (m_resource)
        {
            resource_acquire(m_resource);
        }

        return *this;
    }

    void resource_handle::reset()
    {
        if (m_resource)
        {
            resource_release(m_resource);
        }

        m_ptr = nullptr;
        m_resource = nullptr;
    }

    type_id resource_handle::get_type() const noexcept
    {
        return m_resource->type;
    }
}