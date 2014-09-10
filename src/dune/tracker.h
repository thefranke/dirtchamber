/*
 * dune::tracker by Tobias Alexander Franke (tob@cyberhead.de) 2014
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

/*! \file */

#ifndef DUNE_TRACKER
#define DUNE_TRACKER

#ifdef OPENCV

#include "render_target.h"

#include <DirectXMath.h>

#include <opencv2/core/core.hpp>

namespace dune
{
    /*!
     * \brief A basic pattern.
     *
     * This class encodes a pattern loaded from disk (usually an image), which pre-rotates
     * the pattern for later tracking purposes.
     */
    class pattern
    {
    protected:
        std::vector<cv::Mat> rotated_versions_;

    public:
        /*!
         * \brief Load a pattern from disk.
         *
         * Load a pattern from disk and create pre-rotated versions which are later accessed for tracking.
         *
         * \param filename The pattern's filename.
         */
        void load(const tstring& filename);
        
        const cv::Mat& operator[](size_t i) const { return rotated_versions_[i]; }
    };

    /*!
     * \brief Detected pattern information block.
     *
     * Once a pattern was detected, this structure contains information about the detection, such
     * as the index in the array of patterns which was supplied for detection, the orientation
     * that had the highest correlation as well as the correlation itself, and a transformation
     * matrix from camera to world space which can be used to transform a mesh to the position
     * and orientation of the pattern.
     */
    struct detection_info
    {
        size_t index;
        int orientation;
        double correlation;

        DirectX::XMMATRIX transformation;
    };

    /*!
     * \brief A simple pattern tracker written with OpenCV.
     *
     * This class implements a basic pattern tracker. Patterns are loaded and added to a vector of patterns
     * which are then detected in render_target frames.
     */
    class tracker
    {
    public:
        float time_track_;

    protected:
        cv::Mat cam_intrinsic_;
        cv::Mat cam_distortion_;
        
        std::vector<pattern> patterns_;
        std::vector<detection_info> detected_;

    public:
        void create(const tstring& camera_parameters_filename);
        void destroy();

        /*!
         * \brief Track loaded patterns in a render_target frame.
         * 
         * This method needs to be called whenever a new render_target is available where tracking should be performed.
         * For each loaded pattern, tracking information will be gathered that can be later accessed with the model_view_matrix() method.
         *
         * \param frame A render_target frame which has its data cached
         */
        void track_frame(render_target& frame);

        /*! \brief Load a pattern filename from disk. Returns an ID for the pattern to later identify it. */
        size_t load_pattern(const tstring& filename);

        /*! \brief Remove a loaded pattern identified by an ID. */
        void remove_pattern(size_t id);
        
        /*!
         * \brief Retreive a model-view matrix for a pattern id.
         *
         * This method will construct a model-view matrix necessary to correctly transform 3D geometry according to a
         * tracked pattern. Additionally, rescaling, transformation, and a rotation can be added beforehand. If the pattern
         * wasn't detected in a frame by the method track_frame, then the previously detected matrix will be returned.
         *
         * \param scale Scales the world before moving it to the pattern.
         * \param translation Translate the world before moving it to the pattern.
         * \param rotation Rotate the world before moving it to the pattern.
         * \param pattern_id The ID of the pattern, in case more than one is tracked simultanously.
         * \return The model-view matrix to move the world position to the pattern space.
         */
        DirectX::XMFLOAT4X4 model_view_matrix(FLOAT scale, const DirectX::XMFLOAT3& translation, const DirectX::XMFLOAT4X4& rotation, UINT pattern_id = 0) const;
    };
}

#endif

#endif
