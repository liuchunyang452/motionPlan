#include "A_star.h"
#include "JPS.h"
#include "sample_base.h"
#include <ros/ros.h>
#include <sensor_msgs/PointCloud2.h>
#include <nav_msgs/Path.h>
#include <pcl_conversions/pcl_conversions.h>
#include "path_finding.h"
#include <visualization_msgs/Marker.h>
#include <visualization_msgs/MarkerArray.h>
#include "WPA.h"

void rcvMap_CB(const sensor_msgs::PointCloud2 &map);
void rcvTarget_CB(const nav_msgs::Path &target);
void mapParamConfig(mapParam &param,ros::NodeHandle n);
void visGridPath(std::vector<Eigen::Vector3d> node,int color,visualization_msgs::Marker &node_vis);
void visRRTStarPath(std::vector<Eigen::Vector3d> nodes,visualization_msgs::Marker &Line,visualization_msgs::Marker &Points);

a_star *aStar_Ptr;
sample_base *RRT_Star_Ptr;
WPA *WPA_Ptr;

int main(int argc,char **argv)
{
    //订阅地图信息、目标点信息，设置当前位置为（0,0,0）
    //发布路径、发布调试信息

    ros::init(argc,argv,"path_finding");
    ros::NodeHandle n("~");
    ros::Subscriber map_sub,target_sub;
    ros::Publisher path_pub,line_pub,point_pub;
    map_sub = n.subscribe("/random_map_node/global_map",1,rcvMap_CB);
    target_sub = n.subscribe("/waypoint_generator/waypoints",1,rcvTarget_CB);
    path_pub = n.advertise<visualization_msgs::Marker>("path_vis",1);
    line_pub = n.advertise<visualization_msgs::Marker>("line_vis",1);
    point_pub = n.advertise<visualization_msgs::Marker>("point_vis",1);

    mapParam param;
    mapParamConfig(param,n);
    aStar_Ptr = new a_star(param);
    RRT_Star_Ptr = new sample_base(param,PLANNER_RRTSTAR);
    WPA_Ptr=new WPA(param);

    ros::Rate rate(100);
    bool status = ros::ok();
    ROS_INFO("initial finished ,wait for map");
    while(status)
    {
        aStar_Ptr->findPath();
        WPA_Ptr->findPath();
        //RRT_Star_Ptr->findPath();

        if(!aStar_Ptr->getPath().empty())
        {
            visualization_msgs::Marker node_vis;
            node_vis.scale.x = param.resolution;
            node_vis.scale.y = param.resolution;
            node_vis.scale.z = param.resolution;

            visGridPath(aStar_Ptr->getPath(),RED,node_vis);
            path_pub.publish(node_vis);
        }

        if(!WPA_Ptr->getPath().empty())
        {
            visualization_msgs::Marker Points,Line;
            Points.scale.x = param.resolution/2;
            Points.scale.y = param.resolution/2;
            Line.scale.x   = param.resolution/2;

            visRRTStarPath(WPA_Ptr->getPath(),Line,Points);
            point_pub.publish(Points);
            line_pub.publish(Line);
        }
        /*
        if(!RRT_Star_Ptr->getPath().empty())
        {
            visualization_msgs::Marker Points,Line;
            Points.scale.x = param.resolution/2;
            Points.scale.y = param.resolution/2;
            Line.scale.x   = param.resolution/2;

            visRRTStarPath(RRT_Star_Ptr->getPath(),Line,Points);
            point_pub.publish(Points);
            line_pub.publish(Line);
        }
        */
        ros::spinOnce();
        status = ros::ok();
        rate.sleep();
    }
    return 0;
}
/**
 * @biref 地图信息回调函数，接收地图信息并给算法类赋值
 * @param map 地图数据
 */
void rcvMap_CB(const sensor_msgs::PointCloud2 &map)
{
    static bool has_map = false;
    if(has_map)
    {//只在初始化的时候更新一次地图
        return ;
    }
    has_map = true;
    ROS_INFO("have received the map message!!!");
    //转换成pcl点云的数据格式
    pcl::PointCloud<pcl::PointXYZ> map_pcl;
    pcl::fromROSMsg(map,map_pcl);
    //给算法类设定地图信息
    aStar_Ptr->setMap(map_pcl);
    WPA_Ptr->setMap(map_pcl);
    RRT_Star_Ptr->setMap(map_pcl);
}

/**
 * @brief 目标点的回调函数,接收数据并赋值
 * @param target
 */
void rcvTarget_CB(const nav_msgs::Path &target)
{
    if(target.poses[0].pose.position.z<0.0)
    {
        ROS_WARN("target pose is out of the map !");
        return;
    }
    Eigen::Vector3d target3d;
    target3d << target.poses[0].pose.position.x,
                target.poses[0].pose.position.y,
                target.poses[0].pose.position.z;
    ROS_INFO("get target point success ! x:%f y:%f z:%f",target3d[0],target3d[1],target3d[2]);
    aStar_Ptr->setTarget(target3d);
    WPA_Ptr->setTarget(target3d);
    RRT_Star_Ptr->setTarget(target3d);
}

void mapParamConfig(mapParam &param,ros::NodeHandle n)
{
    n.param("map/cloud_margin",param.cloud_margin,0.0);
    n.param("map/resolution",param.resolution,0.2);

    n.param("map/x_size",param.x_size, 10.0);
    n.param("map/y_size",param.y_size, 10.0);
    n.param("map/z_size",param.z_size, 2.0 );

    param.map_lower << -param.x_size/2,-param.y_size/2,0.0;
    param.map_upper << +param.x_size/2,+param.y_size/2,param.z_size;
    param.inv_resolution = 1.0/param.resolution;

    param.max_x_id = (int)(param.x_size*param.inv_resolution);
    param.max_y_id = (int)(param.y_size*param.inv_resolution);
    param.max_z_id = (int)(param.z_size*param.inv_resolution);
}
void visGridPath(std::vector<Eigen::Vector3d> node,int color,visualization_msgs::Marker &node_vis)
{

    node_vis.header.frame_id = "world";
    node_vis.header.stamp = ros::Time::now();

    node_vis.ns = "astar_path";

    node_vis.type = visualization_msgs::Marker::CUBE_LIST;
    node_vis.action = visualization_msgs::Marker::ADD;
    node_vis.id = 0;

    node_vis.pose.orientation.x = 0.0;
    node_vis.pose.orientation.y = 0.0;
    node_vis.pose.orientation.z = 0.0;
    node_vis.pose.orientation.w = 1.0;

    node_vis.color.a = 1.0;
    node_vis.color.r = 1.0;
    node_vis.color.g = 0.0;
    node_vis.color.b = 0.0;

    geometry_msgs::Point pt;
    for(int i = 0; i < int(node.size()); i++)
    {
        Eigen::Vector3d coord = node[i];
        pt.x = coord(0);
        pt.y = coord(1);
        pt.z = coord(2);

        node_vis.points.push_back(pt);
    }

}

void visRRTStarPath(std::vector<Eigen::Vector3d> nodes,visualization_msgs::Marker &Line,visualization_msgs::Marker &Points)
{
    Points.header.frame_id = Line.header.frame_id = "world";
    Points.header.stamp    = Line.header.stamp    = ros::Time::now();
    Points.ns              = Line.ns              = "demo_node/RRTstarPath";
    Points.action          = Line.action          = visualization_msgs::Marker::ADD;
    Points.pose.orientation.w = Line.pose.orientation.w = 1.0;
    Points.id = 0;
    Line.id   = 1;
    Points.type = visualization_msgs::Marker::POINTS;
    Line.type   = visualization_msgs::Marker::LINE_STRIP;



    //points are green and Line Strip is blue
    Points.color.g = 1.0f;
    Points.color.a = 1.0;
    Line.color.b   = 1.0;
    Line.color.a   = 1.0;

    geometry_msgs::Point pt;
    for(int i = 0; i < int(nodes.size()); i++)
    {
        Eigen::Vector3d coord = nodes[i];
        pt.x = coord(0);
        pt.y = coord(1);
        pt.z = coord(2);

        Points.points.push_back(pt);
        Line.points.push_back(pt);
    }

}

