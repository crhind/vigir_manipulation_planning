function [ posture, success ] = calcIKPosture( visualizer, robot_model, nominal_pose, q0, request )
    nq = robot_model.getNumPositions();

    q_seed = q0;
    q_nom = nominal_pose;

    % comma-separated q0
    %q0_str = sprintf('%f, ', q0);
    
    % show start pose    
    if ( ~isempty(visualizer) )
        visualizer.draw(cputime, q0);
    end

    % build list of constraints from message
    activeConstraints = buildIKConstraints(robot_model, request, q0);

    % run inverse kinematics (mex)
    ikoptions = initIKOptions(robot_model, request.free_joint_names);
    [posture,info_mex,infeasible_constraints] = inverseKin(robot_model,q_seed,q_nom,activeConstraints{:},ikoptions);

    % visualize result
    if ( ~isempty(visualizer) )
        visualizer.draw(cputime, posture);
    end

    if(info_mex>10) % something went wrong
        str = sprintf('SNOPT info is %d, IK mex fails to solve the problem',info_mex);
        ros.log('WARN', str);
        ros.log('INFO', 'Infeasible Constraints:');

        str = sprintf('%s |  ', infeasible_constraints{:});
        ros.log('INFO', str);
        
        success = false;        
    else
        success = true;
    end
end

