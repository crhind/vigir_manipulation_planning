#ifndef VIGIR_HEAD_CONTROL_NODE_H
#define VIGIR_HEAD_CONTROL_NODE_H

#include <ros/ros.h>
#include <vigir_planning_msgs/HeadControlCommand.h>
#include <trajectory_msgs/JointTrajectory.h>
#include <geometry_msgs/PoseStamped.h>
#include <geometry_msgs/QuaternionStamped.h>
#include <tf/transform_listener.h>
#include <tf/transform_datatypes.h>
#include <tf2_msgs/TFMessage.h>

namespace head_control{

    class HeadControl {

    public:
      HeadControl();
      void updateHeadPosition();
    protected:
      void HeadControlCb(const vigir_planning_msgs::HeadControlCommand &command);
      void setHeadJointPosition(const double pan, const double tilt);

      std::vector<double> computeJointsForTracking(const std::string &target_frame_id);
    private:
      unsigned char tracking_mode;
      tf::Vector3 old_target_frame_origin;
      double track_frame_threshold;

      ros::Publisher joint_trajectory_pub;
      ros::Subscriber head_control_sub;
      std::string tracking_frame;
      tf::TransformListener tf;

      std::vector<double> head_cmd; // pan, tilt
      std::vector<std::string> all_frames;
    };
}

//namespace head_tracking_mode{
//  const unsigned char NONE = 0;
//  const unsigned char LEFT_HAND_TRACKING = 1;
//  const unsigned char RIGHT_HAND_TRACKING = 2;
//  const unsigned char FRAME_TRACKING = 3;
//  const unsigned char USE_PROVIDED_JOINTS = 4;
//  const unsigned char LOOK_STRAIGHT = 5;

//}
#endif // VIGIR_HEAD_CONTROL_NODE_H
