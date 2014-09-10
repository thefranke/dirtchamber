/* 
 * dune::cbuffer by Tobias Alexander Franke (tob@cyberhead.de) 2012
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

/*! \file */

#ifndef DUNE_CONSTANT_BUFFER
#define DUNE_CONSTANT_BUFFER

#include "shader_resource.h"
#include "d3d_tools.h"

namespace dune 
{
    /*!
     * \brief A wrapper for constant buffers.
     * 
     * cbuffer is a wrapping class managing a constant buffer. It does so by taking a struct
     * of the constant buffer layout as parameter T, keeping a local copy which can always be
     * accessed and modified. A call to the member update() will copy the local version into
     * the mapped resource. Every call to upload the constant buffer to a shader will automatically
     * update it first.
     */
    template<typename T>
    class cbuffer : public shader_resource
    {
    protected:
        ID3D11Buffer* cb_;
        ID3D11DeviceContext* context_;
        D3D11_MAPPED_SUBRESOURCE mapped_res_;
        T local_;

        T* map(ID3D11DeviceContext* context)
        {
            context_ = context;
            context_->Map(cb_, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_res_);
            return reinterpret_cast<T*>(mapped_res_.pData);
        }

        void unmap()
        {
            if (mapped_res_.pData)
            {
                context_->Unmap(cb_, 0);
                ZeroMemory(&mapped_res_, sizeof(D3D11_MAPPED_SUBRESOURCE));
            }
        }

    public:
        cbuffer() :
            cb_(nullptr),
            context_(nullptr)
        {
            ZeroMemory(&mapped_res_, sizeof(D3D11_MAPPED_SUBRESOURCE));
            ZeroMemory(&local_, sizeof(T));
        }

        virtual ~cbuffer() {}

        T& data()
	    {
		    return local_;
	    }

        /*! \brief Returns a reference to the local copy of the constant buffer. */
        const T& data() const
        {
            return local_;
        }

        void create(ID3D11Device* device)
        {
            D3D11_BUFFER_DESC cbd;
            cbd.Usage = D3D11_USAGE_DYNAMIC;
            cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            cbd.MiscFlags = 0;
            cbd.StructureByteStride = 0;
        
            cbd.ByteWidth = sizeof(T);
            assert_hr(device->CreateBuffer(&cbd, nullptr, &cb_));
        }

	    void to_vs(ID3D11DeviceContext* context, UINT start_slot)
	    {
            update(context);
		    context_->VSSetConstantBuffers(start_slot, 1, &cb_);
	    }

	    void to_gs(ID3D11DeviceContext* context, UINT start_slot)
	    {
            update(context);
		    context_->GSSetConstantBuffers(start_slot, 1, &cb_);
	    }

	    void to_ps(ID3D11DeviceContext* context, UINT start_slot)
	    {
            update(context);
		    context_->PSSetConstantBuffers(start_slot, 1, &cb_);
	    }
	
	    void to_cs(ID3D11DeviceContext* context, UINT start_slot)
	    {
            update(context);
		    context_->CSSetConstantBuffers(start_slot, 1, &cb_);
	    }
    
        /*! \brief Copy over the local buffer into the mapped resource. */
        void update(ID3D11DeviceContext* context)
        {
            T* mapped = map(context);
            {
                *mapped = local_;
            }
            unmap();
        }

        void destroy()
        {
            if (cb_ != nullptr)
            {
                unmap();
                cb_->Release();
            }

            cb_ = nullptr;
            context_ = nullptr;
            ZeroMemory(&mapped_res_, sizeof(D3D11_MAPPED_SUBRESOURCE));
            ZeroMemory(&local_, sizeof(T));
        }
    };
}

#endif