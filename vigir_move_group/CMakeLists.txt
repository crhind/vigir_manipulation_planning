cmake_minimum_required(VERSION 2.8.3)
project(vigir_move_group)

#set(CMAKE_BUILD_TYPE Debug)

## Find catkin macros and libraries
## if COMPONENTS list like find_package(catkin REQUIRED COMPONENTS xyz)
## is used, also find other catkin packages
find_package(catkin REQUIRED COMPONENTS
  cmake_modules
  geometry_msgs
  moveit_msgs
  moveit_ros_move_group
  pluginlib
  roscpp
  vigir_plan_execution
  vigir_moveit_utils
  vigir_planning_msgs
  tf
)

catkin_package(
  INCLUDE_DIRS include
#  LIBRARIES vigir_move_group
#  CATKIN_DEPENDS cmake_modules geometry_msgs moveit_msgs moveit_ros_move_group pluginlib roscpp
#  DEPENDS system_lib
)

include_directories(
  include
  ${catkin_INCLUDE_DIRS}
)

add_library(vigir_move_group_capabilities
  src/continuous_replanning_capability.cpp
  src/manipulation_action_capability.cpp
  src/octomap_access_capability.cpp
  src/robot_state_retrieval_capability.cpp
  src/helper/drake_planning_helper.cpp
  include/vigir_move_group/continuous_replanning_capability.h
  include/vigir_move_group/manipulation_action_capability.h
  include/vigir_move_group/octomap_access_capability.h
  include/vigir_move_group/robot_state_retrieval_capability.h
  include/vigir_move_group/helper/drake_planning_helper.h
  #src/sample_rotational_motion_capability.cpp
  )

target_link_libraries(vigir_move_group_capabilities ${catkin_LIBRARIES} ${Boost_LIBRARIES})

add_dependencies(vigir_move_group_capabilities ${catkin_EXPORTED_TARGETS})

install(TARGETS vigir_move_group_capabilities
  ARCHIVE DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION})

install(DIRECTORY include/ DESTINATION include)

install(FILES
  vigir_move_group_capabilities_plugin_description.xml
  DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION})
