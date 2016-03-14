#ifndef PARTICLE_FILTER_H_
#define PARTICLE_FILTER_H_ PARTICLE_FILTER_H_

#include <vector>

#include <boost/shared_ptr.hpp>

#include "particle.h"
#include "motion_model.h"


class ParticleFilter
{
protected:
    boost::shared_ptr<MotionModel> motion_model_;
    std::vector<Particle> particles_;


public:
    ParticleFilter(boost::shared_ptr<MotionModel> motion_model)
     : motion_model_(motion_model)
    {
    }


    void init(unsigned int n_particles, const tf::Transform& start_pose = tf::Transform::getIdentity())
    {
        particles_.resize(n_particles, Particle(start_pose));
        motion_model_->init(tf::Transform::getIdentity(), particles_);
    }


    void update_motion(const tf::Transform& movement)
    {
        motion_model_->update(movement, particles_);
    }


    tf::Vector3 get_mean()
    {
        tf::Vector3 mean;

        /// \todo consider the weights.
        for (int p = 0; p < particles_.size(); p++)
            mean += particles_[p].get_pose().getOrigin();

        mean /= particles_.size();

        return mean;
    }
};


#endif
