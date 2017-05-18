/**
    SonarPrecipitator Class
    SonarPrecipitator.h
    Purpose: Class that converts `range msg` and `transforms` into PointCloud2

    Adapted from http://docs.ros.org/hydro/api/segbot_sensors/html/range__to__cloud_8cpp_source.html
    See license agreement for adaptation below

    @author Eliot Lim (github: @eliotlim)
    @version 1.0 (16/5/17)
*/

/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (C) 2014, 2015, Jack O'Quin
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
 *   * Neither the name of the author nor other contributors may be
 *     used to endorse or promote products derived from this software
 *     without specific prior written permission.
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

#include <sonar_pointcloud/SonarPrecipitator.h>

using namespace sonar_pointcloud;

/**
    Constructor for SonarPrecipitator
    Instantiates pointcloudFrame and tfListener using a tfBuffer

    @param pointcloudTopic Output topic for PointCloud2
    @param pointcloudFrame TF2 Frame for point cloud origin
*/

SonarPrecipitator::SonarPrecipitator(const std::string& pointcloudTopic, const std::string& pointcloudFrame) :
                                     pointcloudFrame(pointcloudFrame), tfListener(tfBuffer) {
    // ROS Setup
    nodeHandle = ros::NodeHandle();
    pointcloudPublisher = nodeHandle.advertise<sensor_msgs::PointCloud2> (pointcloudTopic, 3);

    // Start publisher thread
    // Refer to http://stackoverflow.com/questions/4581476/using-boost-thread-and-a-non-static-class-function
    publishThread = boost::thread(boost::bind(&SonarPrecipitator::publishCallable, this));
}

/**
    Add a Sonar Input by Topic
    Instantiates a Sonar Object

    @param sonarTopic Input topic for `range_msg`
    @param pointcloudFrame TF2 Frame for Sonar Orientation
*/

void SonarPrecipitator::addSonar(const std::string& sonarTopic, const std::string& sonarFrame) {
    sonars.push_back(Sonar(sonarTopic, sonarFrame));
}

/**
    Publish thread callable
    Loops at specified publishRate, publishing PointCloud2 messages
    Transform Sonar range reading to point cloud about pointcloudFrame
*/

void SonarPrecipitator::publishCallable() {
    ros::Rate loopRate(publishRate);
    while (ros::ok()) {
        pcl::PointCloud<pcl::PointXYZ>::Ptr pointCloud(new pcl::PointCloud<pcl::PointXYZ>());
        pointCloud->header.frame_id = pointcloudFrame;
        pointCloud->height = 1;

        // TODO: Convert all Sonar readings to Points
        std::vector<Sonar>::iterator sonarIt;
        for (sonarIt = sonars.begin(); sonarIt != sonars.end(); sonarIt++) {
            // Check Sonar Range Validity
            if (sonarIt->getRange() < 0) { continue; }
            // Get StampedTransform for Sonar
            geometry_msgs::TransformStamped transform;
            try {
                // Calling lookupTransform with ros::Time(0)
                // results in the latest available transform
                transform = tfBuffer.lookupTransform(pointCloud->header.frame_id,
                                                     sonarIt->sonarFrame,
                                                     ros::Time(0));
            } catch (tf2::TransformException ex) {
                ROS_WARN("Sonar Transform Error: %s", ex.what());
                continue; // skip this reading
            }

            // Transform the range reading into a point
            geometry_msgs::PointStamped pt;
            pt.point.x = sonarIt->getRange();
            geometry_msgs::PointStamped pointOut;
            tf2::doTransform(pt, pointOut, transform);

            // Store the point in cloud
            pcl::PointXYZ pcl_point;
            pcl_point.x = pointOut.point.x;
            pcl_point.y = pointOut.point.y;
            pcl_point.z = pointOut.point.z;
            pointCloud->points.push_back(pcl_point);
            ++(pointCloud->width);

        }

        // Publish PointCloud2
        pointcloudPublisher.publish(pointCloud);
        ROS_DEBUG("PointCloud published with %d elements", pointCloud->width);
        loopRate.sleep();
    }
}