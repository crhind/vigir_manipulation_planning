//=================================================================================================
// Copyright (c) 2014, Stefan Kohlbrecher, TU Darmstadt
// All rights reserved.

// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of the Simulation, Systems Optimization and Robotics
//       group, TU Darmstadt nor the names of its contributors may be used to
//       endorse or promote products derived from this software without
//       specific prior written permission.

// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//=================================================================================================


#include <vigir_move_group/robot_state_retrieval_capability.h>

#include <moveit/planning_pipeline/planning_pipeline.h>

move_group::RobotStateRetrievalCapability::RobotStateRetrievalCapability():
  MoveGroupCapability("RobotStateRetrievalCapability")
{}

void move_group::RobotStateRetrievalCapability::initialize()
{  
  get_planning_group_state_ = root_node_handle_.advertiseService("/flor/planning/upper_body/get_group_state", &RobotStateRetrievalCapability::getCurrentPlanningGroupState, this);
}

bool move_group::RobotStateRetrievalCapability::getCurrentPlanningGroupState(flor_planning_msgs::GetCurrentPlanningGroupState::Request& req, flor_planning_msgs::GetCurrentPlanningGroupState::Response& res)
{
  const robot_model::RobotModelConstPtr& robot_model = context_->planning_pipeline_->getRobotModel();

  if (!robot_model->hasJointModelGroup(req.group_name)){
    ROS_ERROR("Service request for joint states of invalid group %s", req.group_name.c_str());
    return true;
  }

  const robot_state::JointModelGroup* joint_model_group = robot_model->getJointModelGroup(req.group_name);

  {
    planning_scene_monitor::LockedPlanningSceneRO ps(context_->planning_scene_monitor_);
    const robot_state::RobotState& curr_state = context_->planning_scene_monitor_->getPlanningScene()->getCurrentState();

    curr_state.copyJointGroupPositions(joint_model_group, res.position);
  }

  return true;
}

#include <class_loader/class_loader.h>
CLASS_LOADER_REGISTER_CLASS(move_group::RobotStateRetrievalCapability, move_group::MoveGroupCapability)
