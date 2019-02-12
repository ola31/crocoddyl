from crocoddyl import StatePinocchio
from crocoddyl import DifferentialActionModelFloatingInContact
from crocoddyl import IntegratedActionModelEuler
from crocoddyl import CostModelSum
from crocoddyl import CostModelFramePlacement, CostModelFrameVelocity
from crocoddyl import CostModelState, CostModelControl, ActivationModelWeightedQuad
from crocoddyl import ActuationModelFreeFloating
from crocoddyl import ContactModel6D, ContactModelMultiple
from crocoddyl import ShootingProblem, SolverDDP
from crocoddyl import CallbackDDPLogger, CallbackDDPVerbose, CallbackSolverDisplay
from crocoddyl import plotOCSolution, plotDDPConvergence
from crocoddyl import m2a, a2m
from crocoddyl import loadTalosLegs
import numpy as np
from numpy.linalg import norm
import pinocchio
from pinocchio.utils import *


class TaskSE3:
    def __init__(self, oXf, frameId):
        self.oXf = oXf
        self.frameId = frameId


class SimpleBipedWalkingProblem:
    """ Defines a simple 3d locomotion problem
    """
    def __init__(self, rmodel, rightFoot, leftFoot):
        self.rmodel = rmodel
        self.rdata = rmodel.createData()
        self.state = StatePinocchio(self.rmodel)
        self.rightFoot = rightFoot
        self.leftFoot = leftFoot
        # Defining default state
        self.rmodel.defaultState = \
            np.concatenate([m2a(self.rmodel.neutralConfiguration),
                            np.zeros(self.rmodel.nv)])
        # Remove the armature
        self.rmodel.armature[6:] = 1.
    
    def createProblem(self, x, stepLength, stepDuration):
        # Computing the time step per each contact phase given the step duration.
        # Here we assume a constant number of knots per phase
        numKnots = 20
        timeStep = float(stepDuration)/numKnots

        # Getting the frame id for the right and left foot
        rightFootId = self.rmodel.getFrameId(self.rightFoot)
        leftFootId = self.rmodel.getFrameId(self.leftFoot)

        # Compute the current foot positions
        q0 = a2m(x[:self.rmodel.nq])
        pinocchio.forwardKinematics(self.rmodel,self.rdata,q0)
        pinocchio.updateFramePlacements(self.rmodel,self.rdata)
        rightFootPos0 = self.rdata.oMf[rightFootId].translation
        leftFootPos0 = self.rdata.oMf[leftFootId].translation

        # Defining the action models along the time instances
        n_cycles = 2
        loco3dModel = []
        import copy
        for i in range(n_cycles):
            leftSwingModel = \
                [ self.createContactPhaseModel(
                    timeStep,
                    rightFootId,
                    TaskSE3(
                        pinocchio.SE3(np.eye(3),
                                      np.asmatrix(a2m([ [(stepLength*k)/numKnots, 0., 0.] ]) +
                                      leftFootPos0)),
                        leftFootId)
                    ) for k in range(numKnots) ]
            doubleSupportModel = \
                self.createContactSwitchModel(
                    rightFootId,
                    TaskSE3(
                        pinocchio.SE3(np.eye(3),
                                      np.asmatrix(a2m([ stepLength, 0., 0. ]) +
                                      leftFootPos0)),
                        leftFootId)
                    )
            rightSwingModel = \
                [ self.createContactPhaseModel(
                    timeStep,
                    leftFootId,
                    TaskSE3(
                        pinocchio.SE3(np.eye(3),
                                      np.asmatrix(a2m([ 2*(stepLength*k)/numKnots, 0., 0. ]) +
                                      rightFootPos0)),
                        rightFootId)
                    ) for k in range(numKnots) ]
            finalSupport = \
                self.createContactSwitchModel(
                    leftFootId,
                    TaskSE3(
                        pinocchio.SE3(np.eye(3),
                                      np.asmatrix(a2m([ 2*stepLength, 0., 0. ]) +
                                      rightFootPos0)),
                        rightFootId),
                    )
            rightFootPos0 += np.asmatrix(a2m([ stepLength, 0., 0. ]))
            leftFootPos0 += np.asmatrix(a2m([ stepLength, 0., 0. ]))
            loco3dModel += leftSwingModel + [ doubleSupportModel ] + rightSwingModel + [ finalSupport ]

        loco3dModel = leftSwingModel + [ doubleSupportModel ] + rightSwingModel
        problem = ShootingProblem(x, loco3dModel, finalSupport)
        return problem

    def createContactPhaseModel(self, timeStep, contactFootId, footSwingTask):
        # Creating the action model for floating-base systems. A walker system 
        # is by default a floating-base system
        actModel = ActuationModelFreeFloating(self.rmodel)

        # Creating a 6D multi-contact model, and then including the supporting
        # foot
        contactModel = ContactModelMultiple(self.rmodel)
        contactFootModel = \
            ContactModel6D(self.rmodel, contactFootId, ref=pinocchio.SE3.Zero(), gains=[0.,0.])
        contactModel.addContact('contact', contactFootModel)

        # Creating the cost model for a contact phase
        costModel = CostModelSum(self.rmodel, actModel.nu)
        footTrack = CostModelFramePlacement(self.rmodel,
                                        footSwingTask.frameId,
                                        footSwingTask.oXf,
                                        actModel.nu)
        stateWeights = \
            np.array([0]*6 + [0.01]*(self.rmodel.nv-6) + [10]*self.rmodel.nv)
        stateReg = CostModelState(self.rmodel,
                                  self.state,
                                  self.rmodel.defaultState,
                                  actModel.nu,
                                  activation=ActivationModelWeightedQuad(stateWeights**2))
        ctrlReg = CostModelControl(self.rmodel, actModel.nu)
        costModel.addCost("footTrack", footTrack, 100.)
        costModel.addCost("stateReg", stateReg, 0.1)
        costModel.addCost("ctrlReg", ctrlReg, 0.001)

        # Creating the action model for the KKT dynamics with simpletic Euler
        # integration scheme
        dmodel = \
            DifferentialActionModelFloatingInContact(self.rmodel,
                                                     actModel,
                                                     contactModel,
                                                     costModel)
        model = IntegratedActionModelEuler(dmodel)
        model.timeStep = timeStep
        return model

    def createContactSwitchModel(self, contactFootId, swingFootTask):
        model = self.createContactPhaseModel(0., contactFootId, swingFootTask)

        impactFootVelCost = \
            CostModelFrameVelocity(self.rmodel, swingFootTask.frameId)
        model.differential.costs.addCost('impactVel', impactFootVelCost, 10000.)
        model.differential.costs['impactVel' ].weight = 100000
        model.differential.costs['footTrack' ].weight = 100000
        model.differential.costs['stateReg'].weight = 1
        model.differential.costs['ctrlReg'].weight = 0.01
        return model




# Creating the lower-body part of Talos
robot = loadTalosLegs()
rmodel = robot.model

q = robot.q0.copy()
v = zero(robot.model.nv)
x0 = m2a(np.concatenate([q,v]))
x0[2] = 1.019

# Setting up the 3d walking problem
rightFoot = 'right_sole_link'
leftFoot = 'left_sole_link'
walk = SimpleBipedWalkingProblem(rmodel, rightFoot, leftFoot)

# Solving the 3d walking problem using DDP
stepLength = 0.2
stepDuration = 0.75
ddp = SolverDDP(walk.createProblem(x0, stepLength, stepDuration))
cameraTF = [3., 3.68, 0.84, 0.2, 0.62, 0.72, 0.22]
ddp.callback = [CallbackDDPLogger(), CallbackDDPVerbose(),
                CallbackSolverDisplay(robot,4,cameraTF)]
ddp.th_stop = 1e-9
ddp.solve(maxiter=1000,regInit=.1,init_xs=[rmodel.defaultState]*len(ddp.models()))


# Plotting the solution and the DDP convergence
log = ddp.callback[0]
plotOCSolution(log.xs, log.us)
plotDDPConvergence(log.costs,log.control_regs,
                   log.state_regs,log.gm_stops,
                   log.th_stops,log.steps)

# Visualization of the DDP solution in gepetto-viewer
ddp.callback[2](ddp)
#CallbackSolverDisplay(robot)(ddp,cameraTF)