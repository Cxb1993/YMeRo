#pragma once

#include <core/logger.h>
#include <core/datatypes.h>
#include <core/containers.h>

#include "domain.h"

#include <tuple>
#include <vector>
#include <string>
#include <functional>
#include <map>

// Some forward declarations
class ParticleVector;
class ObjectVector;
class CellList;
class TaskScheduler;


class ParticleHaloExchanger;
class ParticleRedistributor;

class ObjectHaloExchanger;
class ObjectRedistributor;
class ObjectForcesReverseExchanger;

class Wall;
class Interaction;
class Integrator;
class InitialConditions;
class Bouncer;
class ObjectBelongingChecker;
class SimulationPlugin;


class CUDA_Cleanup
{
public:
    ~CUDA_Cleanup()
    {
        CUDA_Check( cudaDeviceReset() );
    }
};

class Simulation
{
private:
    /// This member HAS to be THE FIRST defined in the Simulation,
    /// so that it's destroyed the last
    CUDA_Cleanup cleanup;

public:
    int3 nranks3D;
    int3 rank3D;

    MPI_Comm cartComm;
    MPI_Comm interComm;

    DomainInfo domain;

    Simulation(int3 nranks3D, float3 globalDomainSize, const MPI_Comm& comm, const MPI_Comm& interComm, bool gpuAwareMPI);
    ~Simulation();

    void registerParticleVector         (std::unique_ptr<ParticleVector> pv, std::unique_ptr<InitialConditions> ic, int checkpointEvery);
    void registerWall                   (std::unique_ptr<Wall> wall, int checkEvery=0);
    void registerInteraction            (std::unique_ptr<Interaction> interaction);
    void registerIntegrator             (std::unique_ptr<Integrator> integrator);
    void registerBouncer                (std::unique_ptr<Bouncer> bouncer);
    void registerPlugin                 (std::unique_ptr<SimulationPlugin> plugin);
    void registerObjectBelongingChecker (std::unique_ptr<ObjectBelongingChecker> checker);


    void setIntegrator             (std::string integratorName,  std::string pvName);
    void setInteraction            (std::string interactionName, std::string pv1Name, std::string pv2Name);
    void setBouncer                (std::string bouncerName,     std::string objName, std::string pvName);
    void setWallBounce             (std::string wallName,        std::string pvName);
    void setObjectBelongingChecker (std::string checkerName,     std::string objName);


    void applyObjectBelongingChecker(std::string checkerName,
            std::string source, std::string inside, std::string outside, int checkEvery);


    void init();
    void run(int nsteps);
    void finalize();


    std::vector<ParticleVector*> getParticleVectors() const;

    ParticleVector* getPVbyName     (std::string name) const;
    ParticleVector* getPVbyNameOrDie(std::string name) const;
    ObjectVector*   getOVbyNameOrDie(std::string name) const;

    Wall* getWallByNameOrDie(std::string name) const;

    CellList* gelCellList(ParticleVector* pv) const;

    MPI_Comm getCartComm() const;


private:
    const float rcTolerance = 1e-5;

    std::string restartFolder;

    float dt;
    int rank;

    double currentTime;
    int currentStep;

    std::unique_ptr<TaskScheduler> scheduler;

    bool gpuAwareMPI;
    std::unique_ptr<ParticleHaloExchanger> halo;
    std::unique_ptr<ParticleRedistributor> redistributor;

    std::unique_ptr<ObjectHaloExchanger> objHalo;
    std::unique_ptr<ObjectRedistributor> objRedistibutor;
    std::unique_ptr<ObjectForcesReverseExchanger> objHaloForces;

    std::map<std::string, int> pvIdMap;
    std::vector< std::unique_ptr<ParticleVector> > particleVectors;
    std::vector< ObjectVector* >   objectVectors;

    std::map< std::string, std::unique_ptr<Bouncer> >                bouncerMap;
    std::map< std::string, std::unique_ptr<Integrator> >             integratorMap;
    std::map< std::string, std::unique_ptr<Interaction> >            interactionMap;
    std::map< std::string, std::unique_ptr<Wall> >                   wallMap;
    std::map< std::string, std::unique_ptr<ObjectBelongingChecker> > belongingCheckerMap;

    std::map<ParticleVector*, std::vector< std::unique_ptr<CellList> >> cellListMap;

    std::vector<std::tuple<float, ParticleVector*, ParticleVector*, Interaction*>> interactionPrototypes;
    std::vector<std::tuple<Wall*, ParticleVector*>> wallPrototypes;
    std::vector<std::tuple<Wall*, int>> checkWallPrototypes;
    std::vector<std::tuple<Bouncer*, ParticleVector*>> bouncerPrototypes;
    std::vector<std::tuple<ObjectBelongingChecker*, ParticleVector*, ParticleVector*, int>> belongingCorrectionPrototypes;
    std::vector<std::tuple<ObjectBelongingChecker*, ParticleVector*, ParticleVector*, ParticleVector*>> splitterPrototypes;


    std::vector<std::function<void(float, cudaStream_t)>> regularInteractions, haloInteractions;
    std::vector<std::function<void(float, cudaStream_t)>> integratorsStage1, integratorsStage2;
    std::vector<std::function<void(float, cudaStream_t)>> regularBouncers, haloBouncers;

    std::vector< std::unique_ptr<SimulationPlugin> > plugins;

    void prepareCellLists();
    void prepareInteractions();
    void prepareBouncers();
    void prepareWalls();
    void execSplitters();

    void assemble();
};





