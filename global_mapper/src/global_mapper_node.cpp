#include <ros/ros.h>
#include <stdlib.h>
#include <nav_msgs/OccupancyGrid.h>
#include <cv_bridge/cv_bridge.h>
#include <image_transport/image_transport.h>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <geometry_msgs/TransformStamped.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.h>
#include <tf2_ros/transform_listener.h>
#include <sensor_msgs/LaserScan.h>
#include <nav_msgs/Odometry.h>
#include <sensor_msgs/Image.h>
#include <sensor_msgs/image_encodings.h>

#define cellResolution 0.05 //(meters/cell)
#define real_map_width_m 200
#define real_map_height_m 200
#define real_map_width (int)(real_map_width_m/cellResolution)
#define real_map_height (int)(real_map_height_m/cellResolution)
float map_origin_position[3] = {-real_map_width_m/2,-real_map_height_m/2,0};
#define image_scale 160	 //(pixels/meter)

using namespace std;

class real_mapper
{
private:
  ros::NodeHandle nh_;
  ros::Publisher pub_;
  ros::Subscriber sub1_,sub2_;
  
  geometry_msgs::TransformStamped lidar_to_map, cam_to_map;
  nav_msgs::OccupancyGrid real_map;

public:
  tf2_ros::Buffer tfBuffer;
    tf2_ros::TransformListener* tfListener;
  real_mapper(){
  
  real_map.header.frame_id = "odom";
    real_map.header.stamp = ros::Time::now();
    real_map.info.resolution = cellResolution;
    real_map.info.width = real_map_width;
    real_map.info.height = real_map_height;
    real_map.info.origin.position.x = map_origin_position[0];
    real_map.info.origin.position.y = map_origin_position[1];
    tfListener = new tf2_ros::TransformListener(tfBuffer);
    pub_ = nh_.advertise<nav_msgs::OccupancyGrid>("/real_map", 1);
    sub1_ = nh_.subscribe<sensor_msgs::LaserScan>("/scan_1", 1, &real_mapper::scanCallback, this);
    sub2_ = nh_.subscribe("/top_view", 1, &real_mapper::imageCallback, this);
  ROS_INFO("Started real_mapper");
  }

  void scanCallback(const sensor_msgs::LaserScanConstPtr& scan_msg){
    
    lidar_to_map = tfBuffer.lookupTransform("odom", "laser_front", ros::Time(0));
    geometry_msgs::PointStamped point_lidar, point_robot;
    int x,y;
    ROS_INFO("moving to outer loop");
    for(int i=0;i<=(int)((scan_msg->angle_max - scan_msg->angle_min)/scan_msg->angle_increment);i++){
      float laser_angle = scan_msg->angle_min + scan_msg->angle_increment*i;
      if((laser_angle>-M_PI/2 && laser_angle<M_PI) || scan_msg->ranges[i] < scan_msg->range_min || scan_msg->ranges[i] > scan_msg->range_max)
        continue;
      for(float range_current = 0.0; range_current<scan_msg->ranges[i]; range_current+=cellResolution){
        point_lidar.point.x = range_current*cos(laser_angle);
        point_lidar.point.y = range_current*sin(laser_angle);
        point_lidar.point.z = 0.0;
        tf2::doTransform(point_lidar, point_robot, lidar_to_map);
        x = (int)((point_robot.point.x - map_origin_position[0])/cellResolution);
        y = (int)((point_robot.point.y - map_origin_position[1])/cellResolution);
        
        if ((0 <= y )&&(y < real_map_height)&&(0 <= x)&&(x < real_map_width))
          real_map.data[y*real_map_width + x] = 0;

      }
      point_lidar.point.x = scan_msg->ranges[i]*cos(laser_angle);
      point_lidar.point.y = scan_msg->ranges[i]*sin(laser_angle);
      point_lidar.point.z = 0.0;
      tf2::doTransform(point_lidar, point_robot, lidar_to_map);
      x = (int)((point_robot.point.x - map_origin_position[0])/cellResolution);
      y = (int)((point_robot.point.y - map_origin_position[1])/cellResolution);
      if ((0 <= y )&&(y < real_map_height)&&(0 <= x)&&(x < real_map_width))
        real_map.data[y*real_map_width + x] = 100;
    }
  }

  void fillWithUnknown(){
    real_map.data.resize(real_map_width*real_map_height);
     for(int i=0;i<real_map_width*real_map_height;i++){
      real_map.data[i] = -1;
     }
  }

  void imageCallback(const sensor_msgs::ImageConstPtr& msg){
    cam_to_map = tfBuffer.lookupTransform("odom", msg->header.frame_id, ros::Time(0));
    geometry_msgs::PointStamped point_cam, point_robot;
    int x,y;
    cv::Mat gs = cv_bridge::toCvShare(msg, sensor_msgs::image_encodings::MONO8)->image;
   
    for(int i=0;i<gs.rows;i++)
    {
      for(int j=0;j<gs.cols;j++)
      {
        point_cam.point.x = j/(double)image_scale;
        point_cam.point.y = i/(double)image_scale;
        point_cam.point.z = 0.0;
        tf2::doTransform(point_cam, point_robot, cam_to_map);
        x = (int)((point_robot.point.x - map_origin_position[0])/cellResolution);
        y = (int)((point_robot.point.y - map_origin_position[1])/cellResolution);
        if ((0 <= y )&&(y < real_map_height)&&(0 <= x)&&(x < real_map_width)){
        	if (gs.at<uchar>(i, j)==0)
        	{
            	// real_map.data[real_map_height*y + x] = 0;
        	}
          	else
          	{
            	real_map.data[real_map_height*y + x] = 100;
          	}
        }
      }
    }
  }

  void publishMap(){
    pub_.publish(real_map);
  }


};
int main(int argc, char** argv)
{ 
  int loop_count=0;
  ros::init(argc, argv, "real_mapper");
  real_mapper sub_and_pub;
  sub_and_pub.fillWithUnknown();
  ros::Rate loop_rate(10);
  while(ros::ok()){
    if(loop_count%30==0){
      loop_count=0;
      sub_and_pub.publishMap();

    }
    ros::spinOnce();
    loop_rate.sleep();
    loop_count++;
  }
  return 0;
}
