#ifndef ELEVATION_MAP_H_
#define ELEVATION_MAP_H_ ELEVATION_MAP_H_

// Standard library.
#include <vector>
#include <cmath>
#include <iostream>
#include <fstream>

// Point Cloud Library.
#include <pcl/point_cloud.h>

// ROS logging.
#include <ros/console.h>


/// Converts a PCL point cloud into a 2D elevation map.
template<typename PointType>
class ElevationMap
{
protected:
    /// Map data.
    std::vector<std::vector<double> > map_;

    /// Edge length of the map tiles.
    double resolution_;

    /// Minimum admissible resolution.
    const double resolution_min_;

    /// Minimum x coordinate covered by the map.
    double x_min_;

    /// Minimum y coordinate covered by the map.
    double y_min_;


public:
    /// Constructor.
    ElevationMap(const pcl::PointCloud<PointType>& point_cloud, double resolution = 0.1f)
        : resolution_min_(1.0e-3)
    {
        // Set the resolution.
        resolution_ = std::max(resolution_min_, resolution);

        // Compute the limits of the point cloud in x and y direction.
        double x_min = std::numeric_limits<double>::max();
        double y_min = std::numeric_limits<double>::max();
        double x_max = std::numeric_limits<double>::min();
        double y_max = std::numeric_limits<double>::min();
        for (size_t i = 0; i < point_cloud.size(); ++i)
        {
            if (std::isfinite(point_cloud[i].x) && std::isfinite(point_cloud[i].y))
            {
                x_min = std::min<double>(x_min, point_cloud[i].x);
                y_min = std::min<double>(y_min, point_cloud[i].y);
                x_max = std::max<double>(x_max, point_cloud[i].x);
                y_max = std::max<double>(y_max, point_cloud[i].y);
            }
        }

        // Compute the corner of the map where the x and y coordinates reach their minimum.
        x_min_ = std::floor(x_min/resolution) * resolution_;
        y_min_ = std::floor(y_min/resolution) * resolution_;

        // Compute the size of the map.
        size_t x_size = std::max<size_t>(std::ceil<size_t>((x_max-x_min_) / resolution_), 1u);
        size_t y_size = std::max<size_t>(std::ceil<size_t>((y_max-y_min_) / resolution_), 1u);

        // Allocate the map and set all values to NaN.
        map_.resize(x_size);
        for (size_t i = 0; i < map_.size(); ++i)
            map_[i].resize(y_size, std::numeric_limits<double>::quiet_NaN());

        // Compute the elevation values.
        for (size_t i = 0; i < point_cloud.size(); ++i)
        {
            size_t ix, iy;
            if (tile(point_cloud[i], ix, iy))
            {
                if (std::isnan(map_[ix][iy]))
                    map_[ix][iy] = std::numeric_limits<double>::min();

                map_[ix][iy] = std::max<double>(map_[ix][iy], (double)point_cloud[i].z);
            }
        }
    }


    /// Returns the map value correspoding to the given point.
    double elevation(const PointType& point) const
    {
        size_t ix, iy;
        if (tile(point, ix, iy))
            return elevation(ix, iy);
        else
            return std::numeric_limits<double>::quiet_NaN();
    }


    /// Returns the map value corresponding to the given coordinates.
    double elevation(double x, double y) const
    {
        size_t ix, iy;
        if (tile(x, y, ix, iy))
            return elevation(ix, iy);
        else
            return std::numeric_limits<double>::quiet_NaN();
    }


    /// Returns the value of the map tile with the given index.
    double elevation(size_t ix, size_t iy) const
    {
        if (check(ix, iy))
            return map_[ix][iy];
        else
            return std::numeric_limits<double>::quiet_NaN();
    }


    /// Computes the mean distance between two elevation maps.
    double diff(const ElevationMap& map, double d_max = std::numeric_limits<double>::max()) const
    {
        // Check if both maps have the same resolution.
        if (resolution_ != map.resolution_)
            ROS_ERROR("Elevation maps must have the same resolution to be comparable.");

        // Compute the total height distance between the maps.
        double d_total = 0.0;
        size_t n = 0;
        for (size_t ix = 0; ix < map_.size(); ++ix)
            for (size_t iy = 0; iy < map_[0].size(); ++iy)
            {
                // Compute the coordinates of the center of the map tile.
                double x_center = x_min_ + (ix+0.5)*resolution_;
                double y_center = y_min_ + (iy+0.5)*resolution_;

                // Compute the height difference between the two map tiles.
                double d = elevation(x_center, y_center) - map.elevation(x_center, y_center);

                // If the height difference is defined, add it to the total difference.
                if (std::isfinite(d))
                {
                    d_total += std::min(std::abs(d), d_max);
                    n++;
                }
            }

        // Return the mean of the distances.
        if (n < 1)
            return d_max;
        else
            return d_total / n;
    }


    /// Returns the resolution of the map.
    double resolution() const
    {
        return resolution_;
    }


    /// Saves the elevation map to a CSV file.
    void save(std::string filename = std::string())
    {
        // Define the filename.
        if (filename.empty())
        {
            ros::Time now(ros::Time::now());
            std::stringstream filename_stream;
            filename_stream << now.sec << now.nsec << ".csv";
            filename = filename_stream.str();
        }

        // Write the map to a comma-separated file.
        std::ofstream file;
        file.open(filename.c_str());
        for (size_t ix = 0; ix < map_.size(); ++ix)
        {
            for (size_t iy = 0; iy < map_[0].size(); ++iy)
                file << map_[ix][iy] << " ";

            file << std::endl;
        }
        file.close();
    }



protected:
    /// Checks if the given map tile indices are valid.
    bool check(size_t ix, size_t iy) const
    {
        return 0 <= ix && ix < map_.size()
            && 0 <= iy && iy < map_[0].size();
    }

    /// Returns the index of the tile where the given point resides.
    /// If the point lies outside the map, this method returns \c false.
    bool tile(const PointType& point, size_t& ix, size_t& iy) const
    {
        return tile(point.x, point.y, ix, iy);
    }


    /// Returns the index of the tile where the point with the given coordinates resides.
    /// If the point lies outside the map, this method returns \c false.
    bool tile(double x, double y, size_t& ix, size_t& iy) const
    {
        // If the coordinates are infinite, exit immediately.
        if (!std::isfinite(x) || !std::isfinite(y))
            return false;

        // Compute the tile index that corresponds to the given coordinates.
        int ix_tmp = std::floor((x - x_min_) / resolution_);
        int iy_tmp = std::floor((y - y_min_) / resolution_);

        // If the coordinates lie within the map, return the corresponding map tile indices.
        if (check(ix_tmp, iy_tmp))
        {
            ix = ix_tmp;
            iy = iy_tmp;
            return true;
        }
        else
            return false;
    }
};


#endif
