/* 
 * Postprocessing pipeline by Tobias Alexander Franke (tob@cyberhead.de) 2013
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

/*! \file */

#ifndef PPPIPE
#define PPPIPE

#include <dune/cbuffer.h>
#include <dune/postprocess.h>
#include <dune/serializer.h>

/*! 
 * \brief A default implementation of the postprocessor.
 *
 * This is a default implementation of the postprocessor class. The output of a deferred renderer
 * is rendered to the frontbuffer render_target of the postprocessor. Each postprocess effect is rendered
 * by swapping input and output of the previous shader, until the last shader writes its output to the 
 * backbuffer provided to the postprocessor.
 *
 * This postprocessor implements SSAO, Godrays, Depth-of-Field, Bloom, FXAA, exposure adaptation, a CRT monitor
 * effect, TV grain and HDR rendering.
 */
class pppipe : public dune::postprocessor
{
public:
    struct parameters
    {
        BOOL  ssao_enabled;
        float ssao_scale;

        BOOL  godrays_enabled;
        float godrays_tau;

        BOOL  dof_enabled;
        float dof_focal_plane;
        float dof_coc_scale;
    
        BOOL  bloom_enabled;
        float bloom_sigma;
        float bloom_treshold;
    
        BOOL  fxaa_enabled;
    
        BOOL  exposure_adapt;
        float exposure_key;
        float exposure_speed;
    
        BOOL  crt_enabled;
        BOOL  film_grain_enabled;
    };

protected:
    typedef dune::cbuffer<parameters> cb_pp_parameters;
    cb_pp_parameters cb_pp_parameters_;

    ID3D11PixelShader* ssao_;
    ID3D11PixelShader* bloom_;
    ID3D11PixelShader* godrays_;
    ID3D11PixelShader* godrays_merge_;
    ID3D11PixelShader* dof_;
    ID3D11PixelShader* adapt_exposure_;
    ID3D11PixelShader* fxaa_;
    ID3D11PixelShader* bloom_treshold_;
    ID3D11PixelShader* crt_;
    ID3D11PixelShader* film_grain_;

    ID3D11PixelShader* gauss_godrays_h_;
    ID3D11PixelShader* gauss_godrays_v_;
    ID3D11PixelShader* gauss_bloom_h_;
    ID3D11PixelShader* gauss_bloom_v_;
    ID3D11PixelShader* gauss_dof_h_;
    ID3D11PixelShader* gauss_dof_v_;
    ID3D11PixelShader* copy_;

    virtual void do_create(ID3D11Device* device);
    virtual void do_destroy();
    virtual void do_set_shader(ID3D11Device* device);
    virtual void do_resize(UINT width, UINT height);

    dune::render_target blurred_[6];
    dune::render_target bloom_full_;
    dune::render_target frontbuffer_blurred_;

    dune::render_target temporary_;
    dune::render_target rt_adapted_luminance_[2];

    //!@{
    /*! \brief Render depth of field and blur it. */
    void dof(ID3D11DeviceContext* context, dune::render_target& in, dune::render_target& out);
    void dofblur(ID3D11DeviceContext* context, dune::render_target& in, dune::render_target& out);
    //!@}

    //!@{
    /*! \brief Render bloom and blur it. */
    void bloom(ID3D11DeviceContext* context, dune::render_target& frontbuffer);
    void bloomblur(ID3D11DeviceContext* context, dune::render_target& in, dune::render_target& out);
    //!@}

    /*! \brief Compute godrays on halfsize buffer. */
    void godrays(ID3D11DeviceContext* context, dune::render_target& in, dune::render_target& out);
    
    /*! \brief Render the entire pipeline by switching back and forth between a two temporary buffers. */
    void render(ID3D11DeviceContext* context, ID3D11PixelShader* ps, dune::render_target& in, ID3D11RenderTargetView* out);

public:
    virtual void render(ID3D11DeviceContext* context, ID3D11RenderTargetView* backbuffer);

    //!@{
    /*! \brief Return local cbuffer parameters. */
    cb_pp_parameters& parameters() { return cb_pp_parameters_; }
    const cb_pp_parameters& parameters() const { return cb_pp_parameters_; }
    //!@}
};

//!@{
/*! \brief Read/write postprocessor from/to a serializer. */
dune::serializer& operator<<(dune::serializer& s, const pppipe& p);
const dune::serializer& operator>>(const dune::serializer& s, pppipe& p);
//!@}

#endif