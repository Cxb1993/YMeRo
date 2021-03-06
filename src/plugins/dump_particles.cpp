#include <core/simulation.h>
#include <core/pvs/particle_vector.h>
#include <core/utils/folders.h>

#include "dump_particles.h"
#include "simple_serializer.h"


ParticleSenderPlugin::ParticleSenderPlugin(std::string name, std::string pvName, int dumpEvery,
                                           std::vector<std::string> channelNames,
                                           std::vector<ChannelType> channelTypes) :
    SimulationPlugin(name), pvName(pvName),
    dumpEvery(dumpEvery), channelNames(channelNames), channelTypes(channelTypes)
{
    channelData.resize(channelNames.size());
}

void ParticleSenderPlugin::setup(Simulation* simulation, const MPI_Comm& comm, const MPI_Comm& interComm)
{
    SimulationPlugin::setup(simulation, comm, interComm);

    pv = simulation->getPVbyNameOrDie(pvName);

    info("Plugin %s initialized for the following particle vector: %s", name.c_str(), pvName.c_str());
}

void ParticleSenderPlugin::handshake()
{
    std::vector<int> sizes;

    for (auto t : channelTypes)
        switch (t) {
        case ChannelType::Scalar:
            sizes.push_back(1);
            break;
        case ChannelType::Vector:
            sizes.push_back(3);
            break;
        case ChannelType::Tensor6:
            sizes.push_back(6);
            break;
        }

    waitPrevSend();
    SimpleSerializer::serialize(sendBuffer, sizes, channelNames);
    send(sendBuffer);
}

void ParticleSenderPlugin::beforeForces(cudaStream_t stream)
{
    if (currentTimeStep % dumpEvery != 0 || currentTimeStep == 0) return;

    particles.genericCopy(&pv->local()->coosvels, stream);

    for (int i = 0; i < channelNames.size(); ++i) {
        auto name = channelNames[i];
        auto srcContainer = pv->local()->extraPerParticle.getGenericData(name);
        channelData[i].genericCopy(srcContainer, stream); 
    }
}

void ParticleSenderPlugin::serializeAndSend(cudaStream_t stream)
{
    if (currentTimeStep % dumpEvery != 0 || currentTimeStep == 0) return;

    debug2("Plugin %s is sending now data", name.c_str());
    
    for (auto& p : particles)
        p.r = simulation->domain.local2global(p.r);

    debug2("Plugin %s is packing now data", name.c_str());
    SimpleSerializer::serialize(sendBuffer, currentTime, particles, channelData);
    send(sendBuffer);
}




ParticleDumperPlugin::ParticleDumperPlugin(std::string name, std::string path) :
    PostprocessPlugin(name), path(path), positions(new std::vector<float>())
{}

void ParticleDumperPlugin::handshake()
{
    auto req = waitData();
    MPI_Check( MPI_Wait(&req, MPI_STATUS_IGNORE) );
    recv();

    std::vector<int> sizes;
    std::vector<std::string> names;
    SimpleSerializer::deserialize(data, sizes, names);
    
    auto init_channel = [] (XDMF::Channel::Type type, int sz, const std::string& str) {
        return XDMF::Channel(str, nullptr, type);
    };

    // Velocity is a special channel which is always present
    std::string allNames = "velocity";
    channels.push_back(init_channel(XDMF::Channel::Type::Vector, 3, "velocity"));

    for (int i=0; i<sizes.size(); i++)
    {
        allNames += ", " + names[i];
        switch (sizes[i])
        {
            case 1: channels.push_back(init_channel(XDMF::Channel::Type::Scalar,  sizes[i], names[i])); break;
            case 3: channels.push_back(init_channel(XDMF::Channel::Type::Vector,  sizes[i], names[i])); break;
            case 6: channels.push_back(init_channel(XDMF::Channel::Type::Tensor6, sizes[i], names[i])); break;

            default:
                die("Plugin '%s' got %d as a channel '%s' size, expected 1, 3 or 6", name.c_str(), sizes[i], names[i].c_str());
        }
    }
    
    // Create the required folder
    createFoldersCollective(comm, parentPath(path));

    debug2("Plugin '%s' was set up to dump channels %s. Path is %s", name.c_str(), allNames.c_str(), path.c_str());
}

static void unpack_particles(const std::vector<Particle> &particles, std::vector<float> &pos, std::vector<float> &vel)
{
    int n = particles.size();
    pos.resize(3 * n);
    vel.resize(3 * n);

    for (int i = 0; i < n; ++i) {
        auto p = particles[i];
        pos[3*i + 0] = p.r.x;
        pos[3*i + 1] = p.r.y;
        pos[3*i + 2] = p.r.z;

        vel[3*i + 0] = p.u.x;
        vel[3*i + 1] = p.u.y;
        vel[3*i + 2] = p.u.z;
    }
}

float ParticleDumperPlugin::_recvAndUnpack()
{
    float t;
    SimpleSerializer::deserialize(data, t, particles, channelData);
        
    unpack_particles(particles, *positions, velocities);
    
    channels[0].data = velocities.data();
    for (int i = 0; i < channelData.size(); i++)
        channels[i+1].data = channelData[i].data();    
}

void ParticleDumperPlugin::deserialize(MPI_Status& stat)
{
    debug2("Plugin '%s' will dump right now", name.c_str());

    float t = _recvAndUnpack();
    
    std::string fname = path + getStrZeroPadded(timeStamp++, zeroPadding);
    
    XDMF::VertexGrid grid(positions, comm);
    XDMF::write(fname, &grid, channels, t, comm);
}



