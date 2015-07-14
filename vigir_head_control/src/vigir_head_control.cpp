/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2015, Florian Reimold, Dorothea Koert,
 *  TU Darmstadt ( Team ViGIR )
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of TU Darmstadt, Team ViGIR, nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *********************************************************************/

#include <ros/ros.h>
#include <vigir_head_control/vigir_head_control.h>

namespace head_control{
    HeadControl::HeadControl()
    {
        tracking_mode = vigir_planning_msgs::HeadControlCommand::NONE;

        ROS_DEBUG("Creating Head Controler");
        ros::NodeHandle nh_("");
        ros::NodeHandle pnh_("~");

        pnh_.param("track_frame_threshold", track_frame_threshold,0.01);
        head_control_sub = nh_.subscribe("/thor_mang/head_control_mode", 1, &HeadControl::HeadControlCb, this);

        joint_trajectory_pub = nh_.advertise<trajectory_msgs::JointTrajectory>("/thor_mang/head_traj_controller/command", 0, false);

        old_target_frame_origin.setZero();
        head_cmd.resize(2, 0);

        tf.getFrameStrings(all_frames);
    }

    void HeadControl::updateHeadPosition() {
      if(tracking_mode == vigir_planning_msgs::HeadControlCommand::TRACK_FRAME){
          tf.getFrameStrings(all_frames);
          bool frame_exists = false;
          for (int i = 0; i< all_frames.size(); i++){

              if (all_frames[i] == tracking_frame){
                  frame_exists = true;
                  break;
              }

          }
          if (frame_exists) {
              std::vector<double> joints = computeJointsForTracking(tracking_frame);
              if (joints.size() < 2) return;
              setHeadJointPosition(joints[0], joints[1]);
          }

      } else if (tracking_mode == vigir_planning_msgs::HeadControlCommand::TRACK_LEFT_HAND) {
          std::vector<double> joints = computeJointsForTracking("l_hand");
          if (joints.size() < 2) return;
          setHeadJointPosition(joints[0], joints[1]);
      } else if (tracking_mode == vigir_planning_msgs::HeadControlCommand::TRACK_RIGHT_HAND) {
          std::vector<double> joints = computeJointsForTracking("r_hand");
          if (joints.size() < 2) return;
          setHeadJointPosition(joints[0], joints[1]);
      } else if (tracking_mode == vigir_planning_msgs::HeadControlCommand::USE_PROVIDED_JOINTS) {
          setHeadJointPosition(head_cmd[0], head_cmd[1]);
      }
    }

    void HeadControl::HeadControlCb(const vigir_planning_msgs::HeadControlCommand &command) {
     /// //trajectory_msgs/JointTrajectory
        if (tracking_mode != command.motion_type) {
        //ROS_INFO("Setting Head Control Mode to %u", command.motion_type);
        }
        if (command.motion_type == vigir_planning_msgs::HeadControlCommand::NONE) {
            tracking_mode = vigir_planning_msgs::HeadControlCommand::NONE;
        }
        else if (command.motion_type == vigir_planning_msgs::HeadControlCommand::LOOK_STRAIGHT) {
            tracking_mode = vigir_planning_msgs::HeadControlCommand::LOOK_STRAIGHT;
            setHeadJointPosition(0.0, 0.0);
        }
        else if (command.motion_type == vigir_planning_msgs::HeadControlCommand::USE_PROVIDED_JOINTS) {
            tracking_mode = vigir_planning_msgs::HeadControlCommand::USE_PROVIDED_JOINTS;

            if (command.provided_joints.size() < 2) {
                ROS_WARN("Head cannot be moved because values are missing. (%lu provided)", command.provided_joints.size());
            } else {
                head_cmd[0] = std::min(std::max(-1.57, - command.provided_joints[0]), 2.45); // pan
                head_cmd[1] = std::min(std::max(-1.32, command.provided_joints[1]), 0.79); // tilt
            }
        } else if (command.motion_type == vigir_planning_msgs::HeadControlCommand::TRACK_LEFT_HAND) {
            tracking_mode = vigir_planning_msgs::HeadControlCommand::TRACK_LEFT_HAND;
        } else if (command.motion_type == vigir_planning_msgs::HeadControlCommand::TRACK_RIGHT_HAND) {
            tracking_mode = vigir_planning_msgs::HeadControlCommand::TRACK_RIGHT_HAND;
        }else if (command.motion_type == vigir_planning_msgs::HeadControlCommand::TRACK_FRAME) {
            tracking_frame = command.tracking_frame;
            tracking_mode = vigir_planning_msgs::HeadControlCommand::TRACK_FRAME;
        }else{
            ROS_WARN("Received invalid Head Control Mode: %u", command.motion_type);
        }
    }

    void HeadControl::setHeadJointPosition(const double pan, const double tilt){
        trajectory_msgs::JointTrajectory jointTrajectory;

        // Create joint names list
        std::vector<std::string> joints;
        joints.push_back("head_pan");
        joints.push_back("head_tilt");
        jointTrajectory.joint_names = joints;

        // create point list
        std::vector <trajectory_msgs::JointTrajectoryPoint> points;

        trajectory_msgs::JointTrajectoryPoint targetPoint;
        std::vector <double> positions;
        positions.push_back(pan);
        positions.push_back(tilt);
        targetPoint.positions = positions;
        targetPoint.time_from_start = ros::Duration(0.5);

        points.push_back(targetPoint);
        jointTrajectory.points = points;

        ROS_DEBUG("Publishing Message for Head Joints (pan, tilt) = (%f, %f)", pan, tilt);

        joint_trajectory_pub.publish(jointTrajectory);
    }

    std::vector<double> HeadControl::computeJointsForTracking(const std::string &target_frame_id){
        ROS_DEBUG("Computing Joints for tracking %s", target_frame_id.c_str());

        tf::StampedTransform lookat_point_transform;
        tf::StampedTransform base_camera_transform;
        ros::Time now = ros::Time::now();

        try {
            tf.waitForTransform("utorso", target_frame_id, now, ros::Duration(1.0));
            tf.lookupTransform("utorso", target_frame_id, now, lookat_point_transform);
        } catch (std::runtime_error& e) {
            ROS_WARN("Could not transform look_at position to target frame_id %s", e.what());
            return std::vector<double>();
        }


        if ( (old_target_frame_origin.isZero()) || (old_target_frame_origin.distance(lookat_point_transform.getOrigin()) > track_frame_threshold) ){
                old_target_frame_origin = lookat_point_transform.getOrigin();
        }

        try {
            tf.waitForTransform("utorso", "head_link", now, ros::Duration(1.0));
            tf.lookupTransform("utorso", "head_link", now, base_camera_transform);
        } catch (std::runtime_error& e) {
            ROS_WARN("Could not transform from base frame to camera_frame %s", e.what());
            return std::vector<double>();
        }

        tf::Vector3 dir(old_target_frame_origin.x() - base_camera_transform.getOrigin().x(), old_target_frame_origin.y() - base_camera_transform.getOrigin().y(), old_target_frame_origin.z() - base_camera_transform.getOrigin().z());

        double pan = atan2(dir.y(), dir.x()); //yaw
        double tilt = -atan2(dir.z(), sqrt(dir.x()*dir.x() + dir.y()*dir.y()));  // pitch
        pan = std::min(std::max(-1.57, pan), 2.45);
        tilt = std::min(std::max(-1.32, tilt), 0.79);

        std::vector<double> joints;
        joints.push_back(pan);
        joints.push_back(tilt);

        return joints;
    }
}
