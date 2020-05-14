/*  AgentsDynamics
    defines movement rules/differential equations of agents in
    SwarmDynamics(swarmdyn.h, swarmdyn.cpp)
    v0.1, 13.5.2020

    (C) 2020 Pascal Klamser

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "agents_dynamics.h"


void ParticleBurstCoast(particle &a, params * ptrSP, gsl_rng *r){
    double h1 = 0;      // helper
    double dt = ptrSP->dt;
    double forcep = 0.0;
    double forcev = 0.0;
    std::vector<double> force(2);
    double vproj = 0.0;
    double lphi = 0.0;
    int BC = ptrSP->BC;
    double sizeL = ptrSP->sizeL;
    double beta = ptrSP->beta;
    // ----- SET FORCE -----
    // IF: At first burst: set social or environmental force
    // ELSEIF: burst-mode: keep force set at first burst
    // ELSE: coast-mode  : reset force of particle to 0
    bool first_burst = ( a.bin_step == ptrSP->burst_steps );
    bool bursting = ( a.bin_step > 0 );
    double force_mag = ptrSP->soc_strength;
    double prob_social = ptrSP->prob_social;
    double burst_rate = ptrSP->burst_rate;
    double alphaTurn = ptrSP->alphaTurn;
    std::vector<double> hvec(2);

    double draw_social = 0;
    if (first_burst){ // draw if social or environment force
        draw_social = gsl_rng_uniform(r);
        if (draw_social <= prob_social){
            force_mag = ptrSP->soc_strength;
            // implementation corresponds to couzin-model:
            if(a.counter_rep > 0)
                vec_add221(force, a.force_rep);
            else{
                // if both alg AND attraction: weighted average!
                if(a.force_alg[0] != 0 || a.force_alg[1] != 0){
                    // hvec = vec_set_mag(a.force_alg, 1);
                    hvec = vec_set_mag(a.force_alg, a.counter_alg);
                    vec_add221(force, hvec);
                }
                if(a.force_att[0] != 0 || a.force_att[1] != 0){
                    // hvec = vec_set_mag(a.force_att, 1);
                    hvec = vec_set_mag(a.force_att, a.counter_att);
                    vec_add221(force, hvec);
                }
            }
            // normalize:
            if (force[0] != 0 || force[1] != 0)
                force = vec_set_mag(force, 1);
            else{ // if no social-force -> swim straight
                force[0] = cos(a.phi);
                force[1] = sin(a.phi);
            }
        }
        else{ // if no force_flee -> draw random environmental cue
            if ( a.counter_flee > 0 ){
                    force = a.force_flee;
                    force = vec_set_mag(force, 1);
            }
            else {
                force_mag = ptrSP->env_strength;
                lphi = 2 * M_PI * gsl_rng_uniform(r);
                // random direction only forwards:
                // lphi = a.phi + (M_PI * gsl_rng_uniform(r) - M_PI / 2);
                force[0] = cos(lphi);
                force[1] = sin(lphi);
            }
        }
        // WALL: only if not periodic boundary conditions
        if ( ptrSP->BC >= 5){
            double dist2cen = vec_length(a.x);
            double diff = sizeL - dist2cen;
            force = vec_set_mag(force, force_mag);
            a.force = force;
            //------------------ WALL collision avoidance ------------------
            // USE this if increased turning enabled (quick direction change)
            // estimate position at future burst
            // if position is outside of tank: adopt force such that agent is 1BL away from wall 
            // 1: Estimate future position x_fut
            // 2: if x_fut outside of Tank compute collision-point x_coll and:
            // 2.1: compute f_needed to make v parallel to tangent of circle at x_coll
            // 1:
            std::vector<double> x_fut = predictX(a.x, a.v, a.force, 
                                                 dt * ptrSP->burst_steps,
                                                 a.steps_till_burst * dt + ptrSP->burst_steps * dt,
                                                 beta, true);
            h1 = vec_length(x_fut);
            double diff_fut = sizeL - h1;
            hvec = vec_sub(x_fut, a.x);
            h1 = vec_length(hvec);
            if (diff_fut < 0){
                a.force_flee = hvec;
                // 2.:
                // std::vector<double> x_coll(2);
                // x_coll = CircleLineIntersect(sizeL, a.x, x_fut);
                double xphi = atan2(a.x[1], a.x[0]); // position velocity and force will be rotated
                std::vector<double> xx = a.x ;
                std::vector<double> xf = vec_add(a.x, a.force);
                xx = RotateVecCw(xx, xphi);
                xf = RotateVecCw(xf, xphi);
                std::vector<double> x_coll(2);
                if (h1 < sizeL){
                    x_coll = CircleIntersect(sizeL, h1, xx[0]); // r1 (large rad), r2 (smaller rad), x2(center of circle with r2)
                    if (xf[1] < 0) // per default CircleIntersect returns always positive y
                        x_coll[1] *= -1;
                    x_coll = RotateVecCcw(x_coll, xphi);
                    // 2.1.:
                    // hvec = vec_sub(x_coll, a.x);
                    // a.force_flee = hvec;
                    x_coll = vec_set_mag(x_coll, 1);
                    x_coll = vec_perp(x_coll);
                    h1 = vec_dot(a.force, x_coll);
                    force = vec_mul(x_coll, sgn(h1));
                }
                else
                    force = vec_mul(a.x, -1);
            }
        }
        force = vec_set_mag(force, force_mag);
        a.force = force;
    }
    else if (bursting) // burst-mode: keep initial force
        force = a.force;
    else{   // coast-mode: no force
        a.force[0] = force[0] = 0;
        a.force[1] = force[1] = 0;
    }
    if (bursting)
        a.bin_step -= 1;
    if ( a.steps_till_burst > 0 )
        a.steps_till_burst -= 1;
    else
        a.steps_till_burst = 0;
    
    
    // Calculate polar angle
    lphi = a.phi;
    vproj = a.vproj;     // to use correct time-step
    // speed adjustment
    forcev = force[0] * cos(lphi) + force[1] * sin(lphi);
    // a.vproj += (-beta * vproj * vproj * vproj + forcev) * dt;
    a.vproj += (-beta * vproj + forcev) * dt;
    // a.vproj += rnv;
    if (a.vproj < 0){     // prevents F of swimming back
      a.vproj = 0.001;
      lphi += M_PI / 2;
    }
    // normal turn:
    forcep = - force[0] * sin(lphi) + force[1] * cos(lphi);
    lphi += alphaTurn * forcep * dt / vproj;
    // ################ Start Overshoot Check ################
    // due to vproj-dependence extremely large values might occur:
    // overshoot if: 1. Ang(v0, force) < Ang(v1, force)
    //               OR
    //               1. Ang(v0, force) > Ang(v1, force) 
    //               2. Ang(v0, v1) > Ang(v0, force)
    //               OR
    //               1. dphi > pi (maximal angle difference is pi)
    // if overshoot: set direction to force-direction
    std::vector<double> new_u {cos(lphi), sin(lphi)};
    double angForceV0 = acos(vec_dot(a.u, force) / force_mag);
    double angForceV1 = acos(vec_dot(new_u, force) / force_mag);
    double angV0V1 = acos(vec_dot(new_u, a.u));
    bool OvershootI = (angForceV0 < angForceV1);
    bool OvershootII = not OvershootI and (angV0V1 > angForceV0);
    bool OvershootIII = fabs(lphi - a.phi) > M_PI;
    if (fabs(lphi - a.phi) > 0.01 and (OvershootI or OvershootII or OvershootIII))
        lphi = atan2(force[1], force[0]);
    // ################ End Overshoot Check ################
    lphi = fmod(lphi, 2*M_PI);
    lphi = fmod(2*M_PI + lphi, 2*M_PI);   // to ensure positive angle definition
    a.phi = lphi;
    a.u[0] = cos(lphi);
    a.u[1] = sin(lphi);
    
    // Move particles with speed in units of [vel.al. range. / time]
    a.v[0] = a.vproj*a.u[0];
    a.v[1] = a.vproj*a.u[1];
    a.x[0] += a.v[0]*dt;
    a.x[1] += a.v[1]*dt;
    
    // Reset all forces
    // a.force[0]    =a.force[1]=0.0;
    a.force_rep[0] = a.force_rep[1] = 0.0;
    a.force_att[0] = a.force_att[1] = 0.0;
    a.force_alg[0] = a.force_alg[1] = 0.0;
    a.force_flee[0] = a.force_flee[1]=0.0;
    a.counter_rep = 0;
    a.counter_alg = 0;
    a.counter_att = 0;
    a.counter_flee = 0;
    
    // Take into account boundary
    if(BC!=-1)
        Boundary(a, sizeL, BC);
    // ----- Draw time till next burst -----
    if ( a.steps_till_burst == 0 ){
        a.bin_step = ptrSP->burst_steps;
        double draw_update;
        unsigned int steps = 1;
        unsigned int max_steps = 5 / (burst_rate * dt);
        while ( ( a.steps_till_burst == 0 ) ){
            draw_update = gsl_rng_uniform(r);
            if (draw_update <= burst_rate * dt)
                a.steps_till_burst = steps;
            if (steps > max_steps ) // break condition (no infinite loops)
                a.steps_till_burst = steps;
            steps += 1;
        }
    }
    if(BC!=-1)
        Boundary(a, sizeL, BC);
}


template<class agent>
void Boundary(agent &a, double sizeL,  int BC)
{
// Function for calculating boundary conditions
// and update agent position and velocity accordingly
double tx,ty;
double dphi=0.1;
double tmpphi;
double dx=0.0;
double dist2cen;
double diff;
std::vector<double> hv(2);
std::vector<double> wall_normal = a.x;
// -1 is Open boundary condition
switch (BC) {
    case 0:
        // Periodic boundary condition
        a.x[0] = fmod(a.x[0] + sizeL, sizeL);
        a.x[1] = fmod(a.x[1] + sizeL, sizeL);
        break;
    // TODO: any case which changes the velocity should also change u, phi
    case 1:
        // Inelastic box boundary condition
        if (a.x[0]>sizeL)
        {
            a.x[0]=0.9999*sizeL;
            if(a.v[1]>0.)
                tmpphi=(0.5+dphi)*M_PI;
            else
                tmpphi=-(0.5+dphi)*M_PI;

            a.v[0]=cos(tmpphi);
            a.v[1]=sin(tmpphi);
        }
        else if (a.x[0]<0)
        {
            a.x[0]=0.0001;
            if(a.v[1]>0.)
                tmpphi=(0.5-dphi)*M_PI;
            else
                tmpphi=-(0.5-dphi)*M_PI;


            a.v[0]=cos(tmpphi);
            a.v[1]=sin(tmpphi);
        }
        if (a.x[1]>sizeL)
        {
            a.x[1]=0.9999*sizeL;
            if(a.v[0]>0.)
                tmpphi=-dphi;
            else
                tmpphi=M_PI+dphi;

            a.v[0]=cos(tmpphi);
            a.v[1]=sin(tmpphi);

        }
        else if (a.x[1]<0)
        {
            a.x[1]=0.0001;
            if(a.v[0]>0.)
                tmpphi=dphi;
            else
                tmpphi=M_PI-dphi;

            a.v[0]=cos(tmpphi);
            a.v[1]=sin(tmpphi);
        }


        break;
    case 2:
        // Elastic box boundary condition
        if (a.x[0]>sizeL)
        {
            dx=2.*(a.x[0]-sizeL);
            a.x[0]-=dx;
            a.v[0]*=-1.;
        }
        else if (a.x[0]<0)
        {
            dx=2.*a.x[0];
            a.x[0]-=dx;
            a.v[0]*=-1.;
        }
        if (a.x[1]>sizeL)
        {
            dx=2.*(a.x[1]-sizeL);
            a.x[1]-=dx;
            a.v[1]*=-1.;
        }
        else if (a.x[1]<0)
        {
            dx=2.*a.x[1];
            a.x[1]-=dx;
            a.v[1]*=-1.;
        }
        break;

    case 3:
        // Periodic boundary condition in x
        // Elastic  boundary condition in y
        tx= fmod(a.x[0], sizeL);
        if (tx < 0.0f)
            tx += sizeL;
        a.x[0]=tx;

        if (a.x[1]>sizeL)
        {
            dx=2.*(a.x[1]-sizeL);
            a.x[1]-=dx;
            a.v[1]*=-1.;
        }
        else if (a.x[1]<0)
        {
            dx=2.*a.x[1];
            a.x[1]-=dx;
            a.v[1]*=-1.;
        }
        break;
    case 4:
        // Periodic boundary condition in x
        // Inelastic  boundary condition in y
        tx= fmod(a.x[0], sizeL);
        if (tx < 0.0f)
        {
            tx += sizeL;
        }
        a.x[0]=tx;

        if (a.x[1]>sizeL)
        {
            dx=(a.x[1]-sizeL);
            a.x[1]-=dx+0.0001;
            a.v[1]=0.0;
        }
        else if (a.x[1]<0)
        {
            dx=a.x[1];
            a.x[1]-=dx-0.0001;
            a.v[1]=0.0;
        }
        break;
    case 5:
        // Elastic circle boundary condition
        dist2cen = vec_length(a.x);
        diff = dist2cen - sizeL;
        if (diff > 0){
            // 1. mirror the position at the circular wall
            vec_mul221(wall_normal, 1 / dist2cen); // wall_normal = 1 * a.x / |a.x|
            hv = vec_mul(wall_normal, - 2 * diff); // hv = wall_normal * 2 * diff
            vec_add221(a.x, hv);
            // 2. mirror the velocity at the circular wall
            diff = vec_dot(wall_normal, a.v);
            hv = vec_mul(wall_normal, - 2 * diff);
            vec_add221(a.v, hv);
            // 3. update rest of agent properties
            a.u = vec_set_mag(a.v, 1);
            a.phi = atan2(a.u[1], a.u[0]);
        }
        break;
    case 6:
        // half-elastic circle boundary condition
        dist2cen = vec_length(a.x);
        diff = dist2cen - sizeL;
        if (diff > 0){
            // 1. mirror the position at the circular wall
            vec_mul221(wall_normal, 1 / dist2cen); // wall_normal = 1 * a.x / |a.x|
            hv = vec_mul(wall_normal, - 2 * diff); // hv = wall_normal * 2 * diff
            vec_add221(a.x, hv);
            // 2. set velocity component normal to wall to 0
            diff = vec_dot(wall_normal, a.v);
            hv = vec_mul(wall_normal, -diff);
            vec_add221(a.v, hv);
            vec_div221(a.v, 4);
            // 3. update rest of agent properties
            a.u = vec_set_mag(a.v, 1);
            a.phi = atan2(a.u[1], a.u[0]);
        }
        break;
    }
}

template
void Boundary(particle &a, double sizeL,  int BC);
template
void Boundary(predator &a, double sizeL,  int BC);


// Predators represent fishNet: 
//  -line of length < sizeL/2 (otherwise measures are not working)
//  -1.random direction selected
//  -2. identify com
//  -3. create fishNet sizeL/2 away from net in designated direction
void CreateFishNet(std::vector<particle> &a, params *ptrSP,
                   std::vector<predator> &preds, gsl_rng *r){
    std::vector<double> com(2);
    std::vector<double> hv(2);
    std::vector<int> empty(0);
    GetCenterOfMass(a, ptrSP, empty, com);
    double netLength = ptrSP->sizeL / 4;
    double phi = 2 * M_PI * gsl_rng_uniform(r);
    std::vector<double> v_net_move {cos(phi), sin(phi)};
    std::vector<double> v_net_perp = vec_perp(v_net_move);
    double dist2com  = 0.25;
    // dist2com = 0.25 * gsl_rng_uniform(r);
    std::vector<double> xpred_start = vec_mul(v_net_move, - ptrSP->sizeL * dist2com);
    vec_add221(xpred_start, com);   // thats the position L/2 away from COM
    double var_noise = M_PI / 4; 
    double noise = var_noise * gsl_rng_uniform(r) - var_noise / 2;
    phi += noise;
    v_net_move[0] = cos(phi);
    v_net_move[1] = sin(phi);
    v_net_perp = vec_perp(v_net_move);
    hv = vec_mul(v_net_perp, -netLength/2 );
    vec_add221( xpred_start, hv );
    double netDist = netLength / ( preds.size() - 1 );
    for (unsigned int i=0; i<preds.size(); i++){
        hv = vec_mul(v_net_perp, i * netDist);
        preds[i].x = vec_add( xpred_start, hv );
        preds[i].phi = phi;
        preds[i].u = v_net_move;
        preds[i].v[0] = ptrSP->pred_speed0 * preds[i].u[0];
        preds[i].v[1] = ptrSP->pred_speed0 * preds[i].u[1];
        Boundary(preds[i], ptrSP->sizeL, ptrSP->BC);
    }
}


// Predators represent fishing-rot: 
//  -start maximum distance from COM
//  -random start angle
void CreatePredator(std::vector<particle> &a, params *ptrSP,
                    predator &pred, gsl_rng *r){
    // random direction:
    double phi = gsl_rng_uniform(r) * 2 * M_PI;
    pred.phi = phi;
    pred.u[0] = cos(phi);
    pred.u[1] = sin(phi);
    // sizeL/2 (approx max. distance) distance from com:
    std::vector<double> com(2);
    std::vector<double> hv = pred.u;
    std::vector<int> empty(0);
    GetCenterOfMass(a, ptrSP, empty, com);
    vec_mul221(hv, - ptrSP->sizeL/2);
    pred.x = vec_add(com, hv);
    pred.v[0] = ptrSP->pred_speed0 * pred.u[0];
    pred.v[1] = ptrSP->pred_speed0 * pred.u[1];
    Boundary(pred, ptrSP->sizeL, ptrSP->BC);
}


// Blind predator is supposed to have at different speeds the same persistence length:
//  -compare any speed v with v0=1
//      -agent with speed v needs dt=1m/v to travel 1m
//      -it is supposed to have the same angle deviation during 1m as a particle moving with v0
//          -Wiener-process= sqrt(dt) * Dphi
//              - sqrt(1/v0) * Dphi = sqrt(1/v) * \hat{Dphi}
//              - sqrt(1/1) *  Dphi = sqrt(1/v) * \hat{Dphi}
//              -> \hat{Dphi} = sqrt(v) * Dphi
//  -thus must the standard deviation for the angular noise increase with sqrt(v)
void MovePredator(predator &pred, std::vector<particle> &a, params *ptrSP, gsl_rng *r){
    double lphi;
    if (ptrSP->pred_move == 0){
        double rnp = ptrSP->noisep * gsl_ran_gaussian(r, 1.0); //  ptrSP->noisep * gsl_ran_gaussian(r, 1.0);
        lphi = pred.phi;
        lphi +=  rnp * sqrt(ptrSP->pred_speed0); // to keep persistence length the same
        lphi = fmod(lphi, 2*M_PI);
    }
    else{ // directly going for closest prey (no special dynamics)
        std::vector<double> hv(2);
        std::vector<double> minvec = CalcDistVec(pred.x, a[0].x, ptrSP->BC, ptrSP->sizeL); // pointing to a
        double mindist = vec_length(minvec);
        for(int i=1; i<a.size(); i++){
            hv = CalcDistVec(pred.x, a[i].x, ptrSP->BC, ptrSP->sizeL); // pointing to a
            double dist = vec_length(hv);
            if (dist < mindist){
                mindist = dist;
                minvec = hv;
            }
        }
        lphi = atan2(minvec[1], minvec[0]);
        // std::vector<double> com(2);
        // std::vector<int> empty(0);
        // GetCenterOfMass(a, ptrSP, empty, com);
        // vec_sub221(com, pred.x);
        // lphi = atan2(com[1], com[0]);
    }
    pred.phi = lphi;
    pred.u[0] = cos(lphi);
    pred.u[1] = sin(lphi);
    pred.v[0] = ptrSP->pred_speed0 * pred.u[0];
    pred.v[1] = ptrSP->pred_speed0 * pred.u[1];
    pred.x[0] = pred.x[0] + pred.v[0] * ptrSP->dt;
    pred.x[1] = pred.x[1] + pred.v[1] * ptrSP->dt;
    Boundary(pred, ptrSP->sizeL, ptrSP->BC);
    // move to com:
}


void MoveFishNet(std::vector<predator> &preds, params *ptrSP){
    for (unsigned int i=0; i<preds.size(); i++){
        preds[i].x[0] = preds[i].x[0] + preds[i].v[0] * ptrSP->dt;
        preds[i].x[1] = preds[i].x[1] + preds[i].v[1] * ptrSP->dt;
        Boundary(preds[i], ptrSP->sizeL, ptrSP->BC);
    }
}


std::vector<double> predictX(std::vector<double> & x,
                             std::vector<double> & v,
                             std::vector<double> & force,
                             double t_burst, double t_tnb,
                             double friction, bool alongForce){
    // NEGLECTS: 
    //      - velocity perpendicular to force
    // split computation in 2 phases:
    // 1. predicting velocity and position after burst-phase
    // 2. predicting position after coast-phase
    std::vector<double> direction(2);
    double force_len, v0;
    if (alongForce){
        direction = vec_set_mag(force, 1);
        force_len = vec_length(force);
        // v0 = vec_dot(direction, v);
        v0 = vec_length(v);
    }
    else{ // alongVelocity
        direction = vec_set_mag(v, 1);
        force_len = vec_dot(direction, force);
        v0 = vec_length(v);
    }
    // 1.:
    double h0 = exp(-friction * t_burst);
    double h1 = force_len / friction;
    double v1 = h0 * ( v0 - h1 ) + h1;
    double l1 = h0 * ( h1/friction - v0/friction ) + h1 * t_burst + v0/friction - h1/friction;
    // 2.:
    double l2 = v1 / friction * (1 - exp(- friction * ( t_tnb - t_burst ) ));
    vec_mul221(direction, l1 + l2);
    std::vector<double> x_fut = vec_add(direction, x);
    return x_fut;
}


double force2stop(double v, double t_burst, double friction){
    double h = exp(- friction * t_burst);
    double force = - friction * v * h / ( 1 - h );
    return force;
}


// killing of individuals if in kill-range
//      to speed up computation: instead of NxN_p distance computations 
//                               compute if agent in front of net and closer than kill_range
//      assumptions: -sizeL/2 < NetLength
//                   -net is moving perpendicular to its elongation
void FishNetKill(std::vector<particle> &a, std::vector<predator> &preds,
                 params *ptrSP){
    std::vector<double> v_net = CalcDistVec(preds[0].x, preds[1].x, 
                                            ptrSP->BC, ptrSP->sizeL);
    double dist = vec_length(v_net);
    double NetLength = ( preds.size() -1 ) * dist;
    vec_div(v_net, dist);
    std::vector<double> v_net_move = preds[0].u;
    // DEBUGGING
    double zero = fabs( vec_dot( v_net, v_net_move ) );
    if ( zero > 0.0001 )
        std::cout<< "zero: " << zero << std::endl;
    if ( ptrSP->sizeL / 2 < NetLength )  
        std::cout<< "ptrSP->sizeL / 2 < NetLength: FishNetKill not working" << std::endl;
    // compute projection of distance 
    std::vector<double> r_jp(2);
    double front, side; 
    for (unsigned int i=0; i<a.size(); i++){
        r_jp = CalcDistVec(preds[0].x, a[i].x, ptrSP->BC, ptrSP->sizeL); // r_jp = a.x - pred.x -> pointing to a
        front = vec_dot(v_net_move, r_jp);
        side = vec_dot(v_net, r_jp);
        if ( front > 0 && front < ptrSP->kill_range && side > 0 && side <= NetLength ){
            a[i].dead = true;
            ptrSP->Ndead++;
        }
    }
}


// killing of individuals if in kill-range (kill_mode=1)
//                        if in kill-range/sqrt(N_s) (kill_mode=2, confusion)
//                              with N_s as # of agents sensed (r_ip < 2*kill_range)
void PredKill(std::vector<particle> &a, predator &pred,
              params *ptrSP, gsl_rng *r){
    double dist;
    double r_sense = 4 * ptrSP->kill_range;
    if ( ptrSP->pred_kill == 1 ){
        for (unsigned int i=0; i<a.size(); i++){
            dist = CalcDist(pred.x, a[i].x, ptrSP->BC, ptrSP->sizeL);
            if ( dist <= ptrSP->kill_range ){
                a[i].dead = true;
                ptrSP->Ndead++;
            }
        }
    }
    else if ( ptrSP->pred_kill > 1 ){
        std::vector<double> catch_probs;
        catch_probs.reserve(a.size());
        int N_sensed = 0;
        std::vector<unsigned int> possible_kill_id;
        possible_kill_id.reserve(a.size());
        double prob_catched;
        double total_catch_prob = 0;
        for (unsigned int i=0; i<a.size(); i++){
            dist = CalcDist(pred.x, a[i].x, ptrSP->BC, ptrSP->sizeL);
            if ( dist <= r_sense ){
                N_sensed++;
                if ( dist < ptrSP->kill_range ){
                    possible_kill_id.push_back(i);
                    prob_catched = (ptrSP->kill_range - dist) / ptrSP->kill_range; // is always > 0 due to if-condition above
                    catch_probs.push_back( prob_catched );
                    total_catch_prob += prob_catched;
                }
            }
        }
        double effective_kill_rate;
        if ( N_sensed > 0 ) // confusion effect assumed to be 1/N_sensed
            effective_kill_rate = ptrSP->kill_rate * SigmoidFunction( N_sensed, ptrSP->N_confu, -1);
        double prob_killed = 0;
        for (unsigned int i=0; i<possible_kill_id.size(); i++){
            if (ptrSP->pred_kill == 2)          // probabilistic 
                prob_killed = ptrSP->kill_rate * ptrSP->dt;
            else if (ptrSP->pred_kill == 3)     // probabilistic + confusion
                prob_killed = effective_kill_rate * ptrSP->dt;
            else if (ptrSP->pred_kill == 4){    // probabilistic + confusion + selection 
                double prob_selected = catch_probs[i] / total_catch_prob;
                prob_killed = effective_kill_rate * ptrSP->dt * prob_selected;
            }
            double luck = gsl_rng_uniform(r); 
            if ( prob_killed > luck){
                unsigned int ii = possible_kill_id[i];
                a[ii].dead = true;
                ptrSP->Ndead++;
            }
        }
    }
}