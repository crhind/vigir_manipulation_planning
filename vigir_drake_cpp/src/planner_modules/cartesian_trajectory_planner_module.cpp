#include "vigir_drake_cpp/planner_modules/cartesian_trajectory_planner_module.h"

#include "RigidBodyManipulator.h"
#include "RigidBodyConstraint.h"
#include "RigidBodyIK.h"
#include "IKoptions.h"

#include <vigir_planning_msgs/ExtendedPlanningOptions.h>

#include <tf/tf.h>
#include <ros/ros.h>

namespace vigir_drake_cpp {
  
CartesianTrajectoryPlannerModule::CartesianTrajectoryPlannerModule(RigidBodyManipulator *robot_model ) : TrajectoryPlannerModule(robot_model)
{
}

CartesianTrajectoryPlannerModule::~CartesianTrajectoryPlannerModule()
{

}

bool CartesianTrajectoryPlannerModule::plan(vigir_planning_msgs::RequestDrakeCartesianTrajectory &request_message, vigir_planning_msgs::ResultDrakeTrajectory &result_message)
{

    std::vector<Waypoint*> waypoints = extractOrderedWaypoints(request_message);
    
    // stay at q0 for nominal trajectory
    bool received_world_transform = false;
    VectorXd q0 = VectorXd::Zero(this->getRobotModel()->num_positions);
    q0 = messageQs2DrakeQs(q0, request_message.current_state, received_world_transform);

    std::cout << "q0 = " << std::endl;
    MatrixXd printQ = q0;
    printSortedQs(printQ);
    
    int nq = getRobotModel()->num_positions;
    int num_steps = waypoints.size();
    bool success = true;
    
    // run inverse kinematics
    MatrixXd q_sol(nq,0);
    MatrixXd qd_sol(nq,0);
    MatrixXd qdd_sol(nq,0);
    std::vector<double> t_sol;
        
    for ( int i = 1; i < num_steps; i++ ) {
        Waypoint *current_start_point = waypoints[i-1];
        Waypoint *current_target_point = waypoints[i];

        double time_step = (current_target_point->waypoint_time - current_start_point->waypoint_time) / NUM_TIME_STEPS;
        
        // check trajectory every time_step seconds
        std::vector<double> t_vec;

        t_vec.push_back(current_start_point->waypoint_time);
        t_vec.push_back(current_target_point->waypoint_time);
        t_sol.push_back(current_start_point->waypoint_time);
        
        VectorXd qdot_0 = VectorXd::Zero(this->getRobotModel()->num_velocities);
        MatrixXd q_seed = q0.replicate(1, t_vec.size());
        MatrixXd q_nom = q_seed;

        // build list of constraints from message
        double duration = current_target_point->waypoint_time - current_start_point->waypoint_time;
        IKoptions *ik_options = buildIKOptions(duration);

        // build constraints
        std::vector<RigidBodyConstraint*> constraints = buildIKConstraints(request_message, current_start_point, current_target_point, q0);

        // run IK trajectory calculation
        MatrixXd q_sol_part(this->getRobotModel()->num_positions,t_vec.size());
        MatrixXd qd_sol_part(this->getRobotModel()->num_positions,t_vec.size());
        MatrixXd qdd_sol_part(this->getRobotModel()->num_positions,t_vec.size());
    
        int info;
        std::vector<std::string> infeasible_constraints;
        inverseKinTraj(this->getRobotModel(),t_vec.size(),t_vec.data(),qdot_0,q_seed,q_nom,constraints.size(),constraints.data(),q_sol_part,qd_sol_part,qdd_sol_part,info,infeasible_constraints,*ik_options);
        
        int old_q_size = q_sol.cols();
        q_sol.conservativeResize(nq, q_sol.cols() + q_sol_part.cols()-1 );
        qd_sol.conservativeResize(nq, qd_sol.cols() + qd_sol_part.cols()-1 );
        qdd_sol.conservativeResize(nq, qdd_sol.cols() + qdd_sol_part.cols()-1 );
        q_sol.block(0, old_q_size, nq, q_sol_part.cols()-1) = q_sol_part.block(0, 0, nq, q_sol_part.cols()-1);
        q_sol.block(0, old_q_size, nq, qd_sol_part.cols()-1) = qd_sol_part.block(0, 0, nq, qd_sol_part.cols()-1);
        q_sol.block(0, old_q_size, nq, qdd_sol_part.cols()-1) = qdd_sol_part.block(0, 0, nq, qdd_sol_part.cols()-1);

        // add final element at the very end
        if ( i == num_steps-1 ) {
            t_sol.push_back(waypoints[ waypoints.size()-1 ]->waypoint_time);
            q_sol.conservativeResize(nq, q_sol.cols() + 1 );
            qd_sol.conservativeResize(nq, qd_sol.cols() + 1 );
            qdd_sol.conservativeResize(nq, qdd_sol.cols() + 1 );

            q_sol.block(0, q_sol.cols()-1, nq, 1) = q_sol_part.block(0, q_sol_part.cols()-1, nq, 1);
            q_sol.block(0, q_sol.cols()-1, nq, 1) = qd_sol_part.block(0, qd_sol_part.cols()-1, nq, 1);
            q_sol.block(0, q_sol.cols()-1, nq, 1) = qdd_sol_part.block(0, qdd_sol_part.cols()-1, nq, 1);
        }

        bool success = true;
        if(info>=10) { // something went wrong
            std::string constraint_string = "";
            for (auto const& constraint : infeasible_constraints) { constraint_string += (constraint+" | "); }

            ROS_WARN("Step %d / %d: SNOPT info is %d, IK mex fails to solve the problem", i+1, num_steps, info);
            ROS_INFO("Infeasible constraints: %s", constraint_string.c_str());

            success = false;
        }
    }

    if ( success ) {
        // generate spline from result matrices
        double time_step = 1.0 / request_message.trajectory_sample_rate;
        double duration = waypoints[ waypoints.size()-1]->waypoint_time;
        int num_steps = (duration / time_step) + 0.5;
        Eigen::VectorXd response_t(num_steps+1);

        for ( int i = 0; i <= num_steps; i++) {
            response_t(i) = i*time_step;
        }

        std::cout << "result_q = " << std::endl;
        printSortedQs(q_sol);

        std::cout << "result_qd = " << std::endl;
        printSortedQs(qd_sol);

        Eigen::VectorXd interpolated_t = Map<VectorXd>(t_sol.data(), t_sol.size());
        interpolateTrajectory(q_sol, interpolated_t, response_t, q_sol, qd_sol, qdd_sol);



        result_message = buildTrajectoryResultMsg(q_sol, qd_sol, qdd_sol, response_t, request_message.free_joint_names, received_world_transform);
    }
    else {
        // return an invalid message
        result_message.is_valid = false;
    }

    return success;
}

std::vector<RigidBodyConstraint*> CartesianTrajectoryPlannerModule::buildIKConstraints(vigir_planning_msgs::RequestDrakeCartesianTrajectory &request_message, CartesianTrajectoryPlannerModule::Waypoint *start_waypoint, CartesianTrajectoryPlannerModule::Waypoint *target_waypoint, Eigen::VectorXd &q0) {
    std::vector<RigidBodyConstraint*> constraints;

    Vector2d t_span_total;
    t_span_total << start_waypoint->waypoint_time, target_waypoint->waypoint_time;
    Vector2d t_span_target;
    t_span_target << target_waypoint->waypoint_time, target_waypoint->waypoint_time;

    // avoid self-collisions
    if ( request_message.check_self_collisions ) {
            constraints.push_back( new AllBodiesClosestDistanceConstraint(getRobotModel(), 0.01, 1e10, std::vector<int>(), std::set<std::string>(), t_span_total) );
            //constraints.push_back( new MinDistanceConstraint(getRobotModel(), 0.001, std::vector<int>(), std::set<std::string>(), t_span) );
    }

    // keep torso more or less upright
    int torso_body_idx = getRobotModel()->findLinkId("utorso");
    Vector3d axis;
    axis << 0.0, 0.0, 1.0;
    constraints.push_back(new WorldGazeDirConstraint(getRobotModel(), torso_body_idx, axis, axis, 0.1, t_span_total));

    // fixed foot placement
    int l_foot_id = this->getRobotModel()->findLinkId("l_foot");
    int r_foot_id = this->getRobotModel()->findLinkId("r_foot");

    Vector4d hom_foot_pts;
    hom_foot_pts << 0.0,0.0,0.0,1.0;
    Vector3d foot_pts;
    foot_pts << 0.0,0.0,0.0;

    VectorXd v = VectorXd::Zero(this->getRobotModel()->num_velocities);
    this->getRobotModel()->doKinematicsNew(q0, v);
    Vector7d l_foot_pos = this->getRobotModel()->forwardKinNew(foot_pts, l_foot_id, 0, 2, 0).value();
    Vector7d r_foot_pos = this->getRobotModel()->forwardKinNew(foot_pts, r_foot_id, 0, 2, 0).value();

    constraints.push_back( new WorldPositionConstraint(this->getRobotModel(), l_foot_id, hom_foot_pts, l_foot_pos.block<3,1>(0,0), l_foot_pos.block<3,1>(0,0)) );
    constraints.push_back( new WorldPositionConstraint(this->getRobotModel(), r_foot_id, hom_foot_pts, r_foot_pos.block<3,1>(0,0), r_foot_pos.block<3,1>(0,0)) );
    constraints.push_back( new WorldQuatConstraint(this->getRobotModel(), l_foot_id, l_foot_pos.block<4,1>(3,0), 0.0));
    constraints.push_back( new WorldQuatConstraint(this->getRobotModel(), r_foot_id, l_foot_pos.block<4,1>(3,0), 0.0));

    // add quasi static constraint
    MatrixXd l_foot_contact_pts = getRobotModel()->bodies[l_foot_id]->contact_pts;
    MatrixXd r_foot_contact_pts = getRobotModel()->bodies[r_foot_id]->contact_pts;
    QuasiStaticConstraint *quasi_static_constraint = new QuasiStaticConstraint(this->getRobotModel());
    quasi_static_constraint->addContact(1, &l_foot_id, &l_foot_contact_pts);
    quasi_static_constraint->addContact(1, &r_foot_id, &r_foot_contact_pts);
    quasi_static_constraint->setActive(true);
    quasi_static_constraint->setShrinkFactor(0.9);
    constraints.push_back( quasi_static_constraint );

    // add waypoint constraints
    Vector3d goal_position_vec, target_link_axis_vec;
    Vector4d eef_pts, goal_orientation_quat;
    eef_pts << 0.0, 0.0, 0.0, 1.0;
    for ( int target_waypoint_idx = 0; target_waypoint_idx < target_waypoint->target_link_names.size(); target_waypoint_idx++ ) {
        int start_waypoint_idx = -1;
        for ( int j = 0; j < start_waypoint->target_link_names.size(); j++ ) {
            if ( start_waypoint->target_link_names[j].find( target_waypoint->target_link_names[j]) != std::string::npos ) {
                start_waypoint_idx = j;
                break;
            }
        }

        std::string target_link_name = target_waypoint->target_link_names[target_waypoint_idx];

        // get endeffector body ids and points
        int eef_body_id = getRobotModel()->findLinkId(target_link_name);

        // goal position constraint        
        goal_position_vec << target_waypoint->poses[target_waypoint_idx].position.x, target_waypoint->poses[target_waypoint_idx].position.y, target_waypoint->poses[target_waypoint_idx].position.z;
        constraints.push_back( new WorldPositionConstraint(getRobotModel(), eef_body_id, eef_pts, goal_position_vec, goal_position_vec, t_span_target) );

        // goal orientation constraint and straight line between start and target constraint
        if ( start_waypoint_idx >= 0 && start_waypoint->keep_line_and_orientation[start_waypoint_idx] == true ) {
            goal_orientation_quat << start_waypoint->poses[start_waypoint_idx].orientation.w, start_waypoint->poses[start_waypoint_idx].orientation.x, start_waypoint->poses[start_waypoint_idx].orientation.y, start_waypoint->poses[start_waypoint_idx].orientation.z;
            target_link_axis_vec << start_waypoint->target_link_axis[start_waypoint_idx].x, start_waypoint->target_link_axis[start_waypoint_idx].y, start_waypoint->target_link_axis[start_waypoint_idx].z;
        }
        else if ( target_waypoint->keep_line_and_orientation[target_waypoint_idx] == true ) {
            goal_orientation_quat << target_waypoint->poses[target_waypoint_idx].orientation.w, target_waypoint->poses[target_waypoint_idx].orientation.x, target_waypoint->poses[target_waypoint_idx].orientation.y, target_waypoint->poses[target_waypoint_idx].orientation.z;
            target_link_axis_vec << target_waypoint->target_link_axis[target_waypoint_idx].x, target_waypoint->target_link_axis[target_waypoint_idx].y, target_waypoint->target_link_axis[target_waypoint_idx].z;
        }

        if ( request_message.target_orientation_type == vigir_planning_msgs::ExtendedPlanningOptions::ORIENTATION_FULL ) {
           constraints.push_back( new WorldQuatConstraint(getRobotModel(), eef_body_id, goal_orientation_quat, 0, t_span_target));
        }
        else if ( request_message.target_orientation_type == vigir_planning_msgs::ExtendedPlanningOptions::ORIENTATION_AXIS_ONLY ) { // goal axis orientation constraint
           constraints.push_back( new WorldGazeOrientConstraint(getRobotModel(), eef_body_id, target_link_axis_vec, goal_orientation_quat, 0.05, M_PI, t_span_target ));
        }

        if ( start_waypoint_idx >= 0 && start_waypoint->keep_line_and_orientation[start_waypoint_idx] == true ) {
            // line segment
            Vector4d line_start_vec, line_end_vec;
            line_start_vec << start_waypoint->poses[start_waypoint_idx].position.x, start_waypoint->poses[start_waypoint_idx].position.y, start_waypoint->poses[start_waypoint_idx].position.z, 1.0;
            line_end_vec   << target_waypoint->poses[target_waypoint_idx].position.x, target_waypoint->poses[target_waypoint_idx].position.y, target_waypoint->poses[target_waypoint_idx].position.z, 1.0;

            Matrix4Xd line(4,2);
            line << line_start_vec, line_end_vec;
            constraints.push_back( new Point2LineSegDistConstraint(getRobotModel(),eef_body_id,eef_pts,1,line,0.0,0.02,t_span_total) );
        }

    }

    return constraints;
}

std::vector<CartesianTrajectoryPlannerModule::Waypoint*> CartesianTrajectoryPlannerModule::extractOrderedWaypoints(vigir_planning_msgs::RequestDrakeCartesianTrajectory &request_message) {
    std::map<double, Waypoint*> waypoint_map;
    
    // add dummy waypoint for the starting position
    Waypoint *start_waypoint = new Waypoint();
    start_waypoint->waypoint_time = 0.0;
    waypoint_map[0.0] = start_waypoint;
    
    // put waypoints into map sorted by time   
    for ( int i = 0; i < request_message.waypoint_times.size(); i++ ) {
      double current_time = request_message.waypoint_times[i];
      Waypoint *current_waypoint = nullptr;
      if ( waypoint_map.find(current_time) != waypoint_map.end() ) {
          current_waypoint = waypoint_map[current_time];
      }
      else {
          current_waypoint = new Waypoint();
          waypoint_map[current_time] = current_waypoint;
      }
      
      current_waypoint->target_link_names.push_back( request_message.target_link_names[i] );
      current_waypoint->poses.push_back( request_message.waypoints[i] );
      current_waypoint->target_link_axis.push_back( request_message.target_link_axis[i]);
      current_waypoint->waypoint_time = current_time;
      current_waypoint->keep_line_and_orientation.push_back(true);
    }
    
    // put waypoints in vector
    std::vector<Waypoint*> waypoint_vec;
    for( auto it = waypoint_map.begin(); it != waypoint_map.end(); ++it ) {
        waypoint_vec.push_back( it->second );
    }
    
    return waypoint_vec;
}

}

