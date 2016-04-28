/*
 * Dune D3D library - Tobias Alexander Franke 2014
 * For copyright and license see LICENSE
 * http://www.tobias-franke.eu
 */

#include "tracker.h"

#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/calib3d/calib3d.hpp>
#include <opencv2/highgui/highgui.hpp>

#include "exception.h"
#include "common_tools.h"

namespace dune
{
    namespace detail
    {
        typedef std::map<UINT, pattern*> pattern_id_map;
        typedef std::vector<pattern> pattern_vec;
        typedef std::vector<detection_info> detection_vec;

        static const size_t   MAX_PATTERNS      = 100;
        static const int      PATTERN_SIZE      = 10;
        static const int      NORMALIZED_SIZE   = 10;

        static cv::Mat      normalized_pattern;
        static cv::Point2f  normalized_rect[4];

        /*! \brief Initialize static helpers for the tracking code. */
        void init_tracking()
        {
            normalized_pattern = cv::Mat(NORMALIZED_SIZE, NORMALIZED_SIZE, CV_8UC1);

            float fns = static_cast<float>(NORMALIZED_SIZE);

            // the pattern in a normalized view
            normalized_rect[0] = cv::Point2f(0.f, 0.f);
            normalized_rect[1] = cv::Point2f(fns - 1.f, 0.f);
            normalized_rect[2] = cv::Point2f(fns - 1, fns - 1.f);
            normalized_rect[3] = cv::Point2f(0.f, fns - 1.f);
        }

        /*! \brief Convert a pair (R,T) from the OpenCV coordinate system to the one used in The Dirtchamber. */
        void opencv_to_local(DirectX::XMMATRIX& transformation, const DirectX::XMMATRIX& mat_rot, const DirectX::XMMATRIX& mat_trans)
        {
            DirectX::XMFLOAT4X4 xmf_convert_hand
            {
                -1, 0, 0, 0,
                0, 0, 1, 0,
                0, 1, 0, 0,
                0, 0, 0, 1
            };

            DirectX::XMMATRIX convert_hand = DirectX::XMLoadFloat4x4(&xmf_convert_hand);

            // OpenCV has Z pointing down -> rotate mat, then rotate back
            // OpenCV has RH matrices -> premultiply convert_hand
            // OpenCV rotates around Y axis the other way -> transpose
            transformation = DirectX::XMMatrixRotationX(DirectX::XM_PI / 2.f) * DirectX::XMMatrixTranspose(convert_hand * mat_rot) * mat_trans * DirectX::XMMatrixRotationX(-DirectX::XM_PI / 2.f);
        }

        /*! \brief Convert a render_target frame to a grayscale and a binary image. */
        void grayscale_and_binarize(render_target& frame, cv::Mat& binary, cv::Mat& gray, double threshold, int block_size)
        {
            BYTE* bytes = frame.data<BYTE*>();
            auto desc = frame.desc();

            if (desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM || desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)
            {
                cv::Mat src(cv::Size(desc.Width, desc.Height), CV_8UC4, reinterpret_cast<void*>(bytes));
                cvtColor(src, gray, CV_RGBA2GRAY);
            }
            else if (desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM || desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)
            {
                cv::Mat src(cv::Size(desc.Width, desc.Height), CV_8UC4, reinterpret_cast<void*>(bytes));
                cvtColor(src, gray, CV_BGRA2GRAY);
            }
            else
                throw exception(L"Can't convert");

#define TRACK_FAST
#ifndef TRACK_FAST
#define ADAPTIVE_THRESH CV_ADAPTIVE_THRESH_GAUSSIAN_C
#else
#define ADAPTIVE_THRESH CV_ADAPTIVE_THRESH_MEAN_C
#endif
            adaptiveThreshold(gray, binary, 255, ADAPTIVE_THRESH, CV_THRESH_BINARY_INV, block_size, threshold);
            dilate(binary, binary, cv::Mat());
        }

        /*! \brief Calculate extrinsic camera parameters for a pattern p from */
        void calculate_extrinsics(const cv::Point2f imgp[4], const float normalized_size, const cv::Mat& cam_intrinsics, const cv::Mat& cam_distortions, DirectX::XMMATRIX& extrinsics)
        {
            static bool initalized = false;
            static std::vector<cv::Point3f> object_points(4);
            static std::vector<cv::Point2f> image_points(4);

            if (!initalized)
            {
                object_points[0] = cv::Point3f(0.f, 0.f, 0.f);
                object_points[1] = cv::Point3f(normalized_size, 0.f, 0.f);
                object_points[2] = cv::Point3f(normalized_size, normalized_size, 0.f);
                object_points[3] = cv::Point3f(0.f, normalized_size, 0.f);

                initalized = true;
            }

            for (size_t i = 0; i < 4; ++i)
                image_points[i] = imgp[i];

            cv::Mat R;
            cv::Mat T;
            cv::solvePnP(object_points, image_points, cam_intrinsics, cam_distortions, R, T);

            // rot_vec is CV_64F -> turn to CV_32F
            T.convertTo(T, CV_32F);
            R.convertTo(R, CV_32F);

            // transform rotation vector to rotation matrix
            cv::Mat Rm;
            cv::Rodrigues(R, Rm);

            // create DirectX matrix from opencv
            DirectX::XMFLOAT3X3 xmf_rot(&Rm.at<float>(0, 0));

            DirectX::XMMATRIX mat_trans = DirectX::XMMatrixTranslation(T.at<float>(0), -T.at<float>(2), -T.at<float>(1));
            DirectX::XMMATRIX mat_rot = DirectX::XMLoadFloat3x3(&xmf_rot);

            detail::opencv_to_local(extrinsics, mat_rot, mat_trans);
        }

        /*! \brief Identify a pattern from a vector of patterns and return an index, correlation, orientation and transformation for it. */
        detection_info identify_pattern(const cv::Mat& pattern, const pattern_vec& patterns, int normalized_size)
        {
            detection_info info;

            // init with no correlation, no orientation and some random index
            info.correlation = -1.0;
            info.orientation = -1;
            info.index = MAX_PATTERNS;

            double N = static_cast<double>(normalized_size * normalized_size) / 4.0;

            cv::Scalar mean, stddev;
            cv::meanStdDev(pattern, mean, stddev);

            cv::Mat inter = pattern(cv::Range(normalized_size / 4, 3 * normalized_size / 4), cv::Range(normalized_size / 4, 3 * normalized_size / 4));
            double norm_int_2 = std::pow(norm(inter), 2);

            // go through all patterns and find one with the maximum correlation
            for (size_t j = 0; j < patterns.size(); ++j)
            {
                for (size_t i = 0; i < 4; ++i)
                {
                    double const nnn = std::pow(norm(patterns[j][i]), 2);
                    double const mmm = cv::mean(patterns[j][i]).val[0];
                    double nom = inter.dot(patterns[j][i]) - (N * mean.val[0] * mmm);
                    double den = sqrt((norm_int_2 - (N * mean.val[0] * mean.val[0])) * (nnn - (N * mmm * mmm)));
                    double correlation = nom / den;

                    if (correlation > info.correlation)
                    {
                        info.correlation = correlation;
                        info.index = j;
                        info.orientation = static_cast<int>(i);
                    }
                }
            }

            return info;
        }

        /*! \brief Compute a projection of a rectangle rect_pts to a rectangle normalized_rect_pts to warp an image pattern to a new image warped_pattern. */
        void reproject(const cv::Mat& pattern, const cv::Point2f rect_pts[4], const cv::Point2f normalized_rect_pts[4], const cv::Rect& rec, cv::Mat& warped_pattern)
        {
            // calculate mapping
            cv::Mat homography = cv::getPerspectiveTransform(rect_pts, normalized_rect_pts);

            // extract pattern
            cv::Mat sub_img = pattern(cv::Range(rec.y, rec.y + rec.height), cv::Range(rec.x, rec.x + rec.width));

            // warp extracted pattern to normalized pattern
            cv::warpPerspective(sub_img, warped_pattern, homography, cv::Size(warped_pattern.cols, warped_pattern.rows));
        }

        /*!
         * \brief Detect one or more patterns in an image frame.
         *
         * This function detects one pattern of a vector of patterns in a render_target, given that camera parameters (intrinisc and distortion) are known.
         * For each pattern in the vector patterns, the highest correlation is returned, albeit a minimum confidence (set as a constant inside the function)
         * must not be crossed. The function returns a vector of detection_info structures which contain an index to the original pattern vector and further
         * information about the detection including the transformation to world coordinates.
         *
         * \param frame The rendet_target frame which contains the image to track on.
         * \param cam_intrinsics Camera intrinisc parameters read with OpenCV.
         * \param cam_distorition Camera distortion parameters read with OpenCV.
         * \param patterns A vector of patterns which should be detected in an image (one to MAX_PATTERNS).
         * \return A vector of detected pattern information blocks.
         */
        void detect(render_target& frame, const cv::Mat& cam_intrinsics, const cv::Mat& cam_distortions, const pattern_vec& patterns, detection_vec& detected)
        {
            static const double THRESHOLD = 10.0;
            static const int ADAPTIVE_BLOCK_SIZE = 45;
            static const double MINIMUM_CONFIDENCE = 0.4;

            detected.clear();

            // create grayscale and binary image
            cv::Mat img_binary, img_gray;
            grayscale_and_binarize(frame, img_binary, img_gray, THRESHOLD, ADAPTIVE_BLOCK_SIZE);

            std::vector<std::vector<cv::Point>> contours;

            // find contours in binary image
            cv::findContours(img_binary, contours, CV_RETR_LIST, CV_CHAIN_APPROX_SIMPLE);

            for (size_t i = 0; i < contours.size(); ++i)
            {
                cv::Mat countour = cv::Mat(contours[i]);

                // check the perimeter
                const double perimeter = arcLength(countour, true);
                const int avsize = (img_binary.rows + img_binary.cols) / 2;

                if (perimeter >(avsize / 4) && perimeter < (4 * avsize))
                {
                    std::vector<cv::Point> countour_poly;
                    cv::approxPolyDP(countour, countour_poly, perimeter * 0.02, true);

                    // check rectangularity and convexity
                    if (countour_poly.size() == 4 && isContourConvex(cv::Mat(countour_poly)))
                    {
                        // find the upper left vertex
                        double d;
                        double dmin = (4 * avsize * avsize);
                        int v1 = -1;

                        for (int j = 0; j < 4; j++)
                        {
                            d = norm(countour_poly[j]);

                            if (d < dmin)
                            {
                                dmin = d;
                                v1 = j;
                            }
                        }

                        // create a 2D box from the contour
                        int pMinX, pMinY, pMaxY, pMaxX;

                        cv::Point cp = countour_poly[0];
                        pMinX = pMaxX = cp.x;
                        pMinY = pMaxY = cp.y;

                        for (int j = 1; j < 4; ++j)
                        {
                            cp = countour_poly[j];

                            pMinX = std::min(cp.x, pMinX);
                            pMinY = std::min(cp.y, pMinY);

                            pMaxX = std::max(cp.x, pMaxX);
                            pMaxY = std::max(cp.y, pMaxY);
                        }

                        cv::Rect box(pMinX, pMinY, pMaxX - pMinX + 1, pMaxY - pMinY + 1);

                        // create new vertices with sub pixel accuracy
                        std::vector<cv::Point2f> float_vertices(4);

                        for (size_t j = 0; j < 4; ++j)
                            float_vertices[j] = countour_poly[j];

                        cv::cornerSubPix(img_gray, float_vertices, cv::Size(3, 3), cv::Size(-1, -1), cv::TermCriteria(1, 3, 1));

                        cv::Point2f rect[4];

                        // rotate vertices based on upper left vertex; this gives you the most trivial homogrpahy
                        for (int j = 0; j < 4; ++j)
                            rect[j] = cv::Point2f(float_vertices[(4 + v1 - j) % 4].x - pMinX, float_vertices[(4 + v1 - j) % 4].y - pMinY);

                        // warp the extracted area to a normalized box
                        reproject(img_gray, rect, normalized_rect, box, normalized_pattern);

                        // find some pattern p
                        detection_info p = identify_pattern(normalized_pattern, patterns, NORMALIZED_SIZE);

                        // if the correlation is high enough this pattern is a match
                        if (p.correlation > MINIMUM_CONFIDENCE)
                        {
                            cv::Point2f vertices[4];

                            for (int j = 0; j < 4; ++j)
                                vertices[j] = float_vertices[(8 - p.orientation + v1 - j) % 4];

                            calculate_extrinsics(vertices, static_cast<float>(NORMALIZED_SIZE), cam_intrinsics, cam_distortions, p.transformation);

                            detected.push_back(p);
                        }
                    }
                }
            }
        }
    }

    void pattern::load(const tstring& filename)
    {
        cv::Mat img = cv::imread(to_string(dune::make_absolute_path(filename)), 0);

        if (img.cols != img.rows)
            throw exception(L"Tracking pattern" + filename + L" not square");

        if (img.empty())
            throw exception(L"Couldn't load pattern " + filename);

        int msize = detail::PATTERN_SIZE;

        cv::Mat img_resize(msize, msize, CV_8UC1);
        cv::Point2f center((msize - 1) / 2.0f, (msize - 1) / 2.0f);

        cv::resize(img, img_resize, cv::Size(msize, msize));
        cv::Mat img_sub = img_resize(cv::Range(msize / 4, 3 * msize / 4), cv::Range(msize / 4, 3 * msize / 4));
        rotated_versions_.push_back(img_sub);

        for (int i = 1; i < 4; ++i)
        {
            cv::Mat img_rot = cv::Mat(msize, msize, CV_8UC1);
            cv::Mat rot_mat = cv::getRotationMatrix2D(center, -i * 90, 1.0);
            cv::warpAffine(img_resize, img_rot, rot_mat, cv::Size(msize, msize));

            img_sub = img_rot(cv::Range(msize / 4, 3 * msize / 4), cv::Range(msize / 4, 3 * msize / 4));
            rotated_versions_.push_back(img_sub);
        }
    }

    void tracker::create(const tstring& camera_parameters_filename)
    {
        detail::init_tracking();

        std::string cfstr = to_string(make_absolute_path(camera_parameters_filename));

        cv::FileStorage fs(cfstr.c_str(), cv::FileStorage::READ);
        fs["intrinsic"] >> cam_intrinsic_;
        fs["distortion"] >> cam_distortion_;
    }

    void tracker::destroy()
    {
        patterns_.clear();
        detected_.clear();
    }

    DirectX::XMFLOAT4X4 tracker::model_view_matrix(FLOAT scale, const DirectX::XMFLOAT3& translation, const DirectX::XMFLOAT4X4& rotation, UINT marker_id) const
    {
        static DirectX::XMMATRIX transformation = DirectX::XMMatrixIdentity();

        if (detected_.size() > marker_id)
            transformation = detected_[0].transformation;

        // add calibration
        DirectX::XMMATRIX mat_trans = DirectX::XMMatrixTranslation(translation.x, translation.y, translation.z);
        DirectX::XMMATRIX mat_scale = DirectX::XMMatrixScaling(scale, scale, scale);
        // TODO: add rotation
        DirectX::XMMATRIX the_matrix = mat_scale * mat_trans * transformation;

        DirectX::XMFLOAT4X4 ret;
        DirectX::XMStoreFloat4x4(&ret, the_matrix);

        return ret;
    }

    void tracker::track_frame(render_target& frame)
    {
        int64 start = cvGetTickCount();

        detail::detect(frame, cam_intrinsic_, cam_distortion_, patterns_, detected_);

        int64 end = cvGetTickCount();

        time_track_ = static_cast<float>(end - start) / static_cast<float>(cvGetTickFrequency() * 1000);
    }

    size_t tracker::load_pattern(const tstring& filename)
    {
        pattern p;

        p.load(filename);
        patterns_.push_back(p);

        // this should return the correct ID, as patterns which can't be loaded throw an exception
        return patterns_.size() - 1;
    }
}
