/**
* This file is part of ORB-SLAM.
*
* Copyright (C) 2014 Raúl Mur-Artal <raulmur at unizar dot es> (University of Zaragoza)
* For more information see <http://webdiis.unizar.es/~raulmur/orbslam/>
*
* ORB-SLAM is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* ORB-SLAM is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with ORB-SLAM. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef TRACKING_H
#define TRACKING_H

#include "MotionModel.hpp"
#include "config.h"
#include "g2o_types/anchored_points.h"
#include "g2o_types/IMU_constraint.h"
#include <g2o/core/block_solver.h> //for sparseoptimizer

#include <viso2/p_match.h> //for matches adopted from libviso2
#include <viso2/viso_stereo.h>//for viso2

#include "stereoSFM.h"
#include<opencv2/core/core.hpp>
#include<opencv2/features2d/features2d.hpp>

#include"FramePublisher.h"
#include"Map.h"
#include"LocalMapping.h"
#include"LoopClosing.h"
#include"Frame.h"
#include "ORBVocabulary.h"
#include"KeyFrameDatabase.h"
#include"ORBextractor.h"
#include "Initializer.h"
#include "MapPublisher.h"

#ifdef SLAM_USE_ROS
#include<sensor_msgs/Image.h>
#include<sensor_msgs/image_encodings.h>
#include<tf/transform_broadcaster.h>
#endif

#include <deque> //for temporal window of frames

namespace ORB_SLAM
{

class FramePublisher;
class Map;
class LocalMapping;
class LoopClosing;

class Tracking
{  
public:
#ifdef SLAM_USE_ROS
    Tracking(ORBVocabulary* pVoc,FramePublisher* pFramePublisher, MapPublisher* pMapPublisher,
             Map* pMap, string strSettingPath);
#else
    Tracking(ORBVocabulary* pVoc,FramePublisher* pFramePublisher, /*MapPublisher* pMapPublisher,*/
             Map* pMap, string strSettingPath);
#endif
    ~Tracking();
    enum eTrackingState{
        SYSTEM_NOT_READY=-1,
        NO_IMAGES_YET=0,
        NOT_INITIALIZED=1,
        INITIALIZING=2,
        WORKING=3,
        LOST=4
    };
    double GetFPS() const {return (double)mFps;}
    void SetLocalMapper(LocalMapping* pLocalMapper);
    void SetLoopClosing(LoopClosing* pLoopClosing);
    void SetKeyFrameDatabase(KeyFrameDatabase* pKFDB);
    bool isInTemporalWindow(const Frame* pFrame)const;
    long unsigned int GetCurrentFrameId() {return mpCurrentFrame->mnId;}
    // This is the main function of the Tracking Thread
    void Run();

    void ForceRelocalisation(const g2o::Sim3 );
    Sophus::Sim3d GetSnew2old(){
        return Sophus::Sim3d(Sophus::RxSO3d(mSneww2oldw.scale(), Sophus::SO3d(mSneww2oldw.rotation())), mSneww2oldw.translation());
    }
    Sophus::SE3d GetTnew2old(){
        return Sophus::SE3d(mSneww2oldw.rotation(), mSneww2oldw.translation()/mSneww2oldw.scale());
    }

    void CheckResetByPublishers();
    void setCoreKfs(std::vector<KeyFrame*> &);
    /// Optimize some of the observed 3D points.
    void optimizeStructure(FramePtr frame, size_t max_n_pts, int max_iter);
    vk::PinholeCamera* GetCameraModel(){return cam_;}
    eTrackingState mState;
    eTrackingState mLastProcessedState;    

    // Current Frame
    Frame* mpCurrentFrame;
    Frame* mpLastFrame; //last left frame
    FramePtr mpCurrentRightFrame;
    // Initialization Variables
    std::vector<int> mvIniLastMatches;
    std::vector<int> mvIniMatches;
    std::vector<cv::Point2f> mvbPrevMatched;
    std::vector<cv::Point3f> mvIniP3D;
    Frame* mpInitialFrame;
protected:
    // monocular processing
    void GrabImage(cv::Mat& im, double timeStampSec);

    //process stereo image pair, left_img and right_img, they shared the same time in seconds,
    //time_pair[1], time_pair[0] is for the previous frame
    //in the following two versions, the original implementation ProcessFrame gives the best result
    void ProcessFrame(cv::Mat &left_img, cv::Mat &right_img, double timeStampSec,
                      const std::vector<Eigen::Matrix<double, 7,1> >& imu_measurements = std::vector<Eigen::Matrix<double, 7,1> >(),
                      const Sophus::SE3d * pTcp=NULL, const Eigen::Matrix<double, 9,1> sb=Eigen::Matrix<double, 9,1>::Zero());

    void ProcessFrameQCV(cv::Mat &left_img, cv::Mat &right_img, double timeStampSec,
                      const std::vector<Eigen::Matrix<double, 7,1> >& imu_measurements = std::vector<Eigen::Matrix<double, 7,1> >(),
                      const Sophus::SE3d * pTcp=NULL, const Eigen::Matrix<double, 9,1> sb=Eigen::Matrix<double, 9,1>::Zero());
/// cam_id =0 left image, 1, right image, identity the camera sensor that captures im,
/// pTcp is predicted transform to current image from previous image
/// imu_measurements are the measurements between previous image epoch and current image epoch (k+1)
    void  ProcessFrameSVO(cv::Mat &im, cv::Mat & right_im, double timeStampSec,
                          const std::vector<Eigen::Matrix<double, 7,1> >& imu_measurements = std::vector<Eigen::Matrix<double, 7,1> >(),
                          const Sophus::SE3d * pTcp=NULL, const Eigen::Matrix<double, 9,1> sb=Eigen::Matrix<double, 9,1>::Zero());

    // monocular and imu integration
    void  ProcessFrameMono(cv::Mat &im, double timeStampSec,
                           const std::vector<Eigen::Matrix<double, 7,1> >& imu_measurements = std::vector<Eigen::Matrix<double, 7,1> >(),
                           const Sophus::SE3d * pTcp=NULL, const Eigen::Matrix<double, 9,1> sb=Eigen::Matrix<double, 9,1>::Zero());

    void FirstInitialization();

    void Initialize();
    void CreateInitialMap(Eigen::Matrix3d Rcw, Eigen::Vector3d tcw, double norm_tcinw=0);
    void CreateInitialMapStereo(const Sophus::SE3d &Tcw, const std::vector<p_match> &vQuadMatches);

    void Reset();

    bool TrackPreviousFrame();
    bool TrackPreviousFrame(const Sophus::SE3d& Tcp, const std::vector<p_match>&vQuadMatches);
    bool TrackWithMotionModel();

    bool RelocalisationRequested();
    bool Relocalisation();

    void UpdateReference();
    void UpdateReferencePoints();
    void UpdateReferenceKeyFrames();
    void UpdateReferenceKeyFramesAndPoints();

    bool TrackLocalMap();
    bool TrackLocalMapDWO();
    int SearchReferencePointsInFrustum();
    int SearchReferencePointsInFrustumStereo();

    void setupG2o(ScaViSLAM::G2oCameraParameters * g2o_cam,
                  ScaViSLAM::G2oCameraParameters * g2o_cam_right,
               ScaViSLAM::G2oIMUParameters * g2o_imu,
               g2o::SparseOptimizer * optimizer) const;
    ScaViSLAM::G2oVertexSE3* addPoseToG2o(const Sophus::SE3d & T_me_from_w,
                   int pose_id,
                   bool fixed,
                   g2o::SparseOptimizer * optimizer,
                      const Sophus::SE3d* first_estimate=NULL) const;
    ScaViSLAM::G2oVertexSpeedBias* addSpeedBiasToG2o(const Eigen::Matrix<double, 9,1> & vinw_bias,
                   int sb_id,
                   bool fixed,
                   g2o::SparseOptimizer * optimizer,
                           const Eigen::Matrix<double, 9,1> * first_estimate= NULL) const;
    size_t copyAllPosesToG2o(g2o::SparseOptimizer * optimizer);

 	ScaViSLAM::G2oVertexPointXYZ* addPointToG2o( MapPoint* pPoint,
                    int g2o_point_id, bool fixed,
                    g2o::SparseOptimizer * optimizer) const;

    ScaViSLAM::G2oEdgeProjectXYZ2UV* addObsToG2o(const Eigen::Vector2d & obs, const Eigen::Matrix2d & Lambda,
                  ScaViSLAM::G2oVertexPointXYZ*, ScaViSLAM::G2oVertexSE3*, bool robustify,  double huber_kernel_width,
                  g2o::SparseOptimizer * optimizer, Sophus::SE3d * pTs2c=NULL);
    int  LocalOptimize();
    int  LocalOptimizeSVO();
    bool NeedNewKeyFrame();
    bool NeedNewKeyFrameStereo();
    void CreateNewKeyFrame();
    void CreateNewMapPoints(const std::vector<p_match>& vQuadMatches);
    void CreateNewMapPoints(KeyFrame* pPenultimateKF, KeyFrame* pLastKF);
    //Other Thread Pointers
    LocalMapping* mpLocalMapper;
    LoopClosing* mpLoopClosing;

    //ORB
    ORBextractor* mpORBextractor;
    ORBextractor* mpIniORBextractor; //not used in stereo case
 
    //BoW
    ORBVocabulary* mpORBVocabulary;
    KeyFrameDatabase* mpKeyFrameDB;

    // Initalization
    Initializer* mpInitializer;

    //Local Map
    KeyFrame* mpReferenceKF;
    std::vector<KeyFrame*> mvpLocalKeyFrames;
    std::vector<MapPoint*> mvpLocalMapPoints;

    std::vector<KeyFrame*> mvpOldLocalKeyFrames;
    std::deque <Frame*> mvpTemporalFrames;
    const size_t mnTemporalWinSize; // the current frame and its previous frame is not counted here
    const int mnSpatialWinSize; // keyframes in the temporal window is not counted
    //Publishers
    FramePublisher* mpFramePublisher;
    MapPublisher* mpMapPublisher;

    //Map
    Map* mpMap;

    cv::FileStorage mfsSettings;
    // viso2
    libviso2::VisualOdometryStereo mVisoStereo; // visual odometry 
    libviso2::Matrix mPose; // transform from current frame to world frame  

    // external saved odometry
    StereoSFM mStereoSFM;
    Eigen::Vector3d ginw; //gravity (3-float-column vector) in world frame which is a specific camera frame,
    // for gravity aligned feature descriptor. N.B., it is empty if mbUseIMUData is false

    Sophus::SE3d mTl2r;
    vk::PinholeCamera* cam_;                     //!< Camera model, can be ATAN, Pinhole or Ocam (see vikit).
    vk::PinholeCamera* right_cam_;                     //!< Camera model, can be ATAN, Pinhole or Ocam (see vikit).

    float mFps;
    //New KeyFrame rules (according to fps)
    int mMinFrames;
    int mMaxFrames;

    //Current matches in frame
    int mnMatchesInliers;

    //Last Frame, KeyFrame and Relocalisation Info
    KeyFrame* mpLastKeyFrame;

    unsigned int mnLastKeyFrameId;
    unsigned int mnLastRelocFrameId;

    //Mutex  
    boost::mutex mMutexForceRelocalisation;

    //Reset
    bool mbPublisherStopped;
    bool mbReseting;
    boost::mutex mMutexReset;

    //Is relocalisation requested by an external thread? (loop closing)
    bool mbForceRelocalisation;
    g2o::Sim3 mSneww2oldw; // let $S_w^{c_{old}}$ and $S_w^{c_{new}}$ denote
    // pose of the current keyframe before and after loop optimization, then mSneww2oldw is $(S_w^{c_{old}})^{-1}S_w^{c_{new}}$

    //Motion Model
    Sophus::SE3d mVelocity; //T prev to curr

    //Color order (true RGB, false BGR, ignored if grayscale)
    bool mbRGB;

#ifdef SLAM_USE_ROS
    // Transfor broadcaster (for visualization in rviz)
    tf::TransformBroadcaster mTfBr;
#endif
    //IMU related parameters
    bool mbUseIMUData;
    double imu_sample_interval;             //sampling interval in second
    ScaViSLAM::G2oIMUParameters imu_;
    static const int MAGIC2=2;         //we use 2*i to identify pose vertices and 2*i+1 for speeb bias vertices in g2o optimizer
    int mnStartId; // used to offset the ID of frames

    const int mnFeatures;// how many point features in a frame

    PointStatistics point_stats;
    std::vector<KeyFrame*> core_kfs_;                      //!< Keyframes in the closer neighbourhood.
    MotionModel mMotionModel;

};

} //namespace ORB_SLAM

#endif // TRACKING_H
