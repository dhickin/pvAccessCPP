/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <stdexcept>

#define epicsExportSharedSymbols
#include <pv/rpcServer.h>

using namespace epics::pvData;
using std::string;

namespace epics { namespace pvAccess {


class ChannelRPCServiceImpl :
    public ChannelRPC,
    public std::tr1::enable_shared_from_this<ChannelRPCServiceImpl>
{
    private:
    Channel::shared_pointer m_channel;
    ChannelRPCRequester::shared_pointer m_channelRPCRequester;
    RPCService::shared_pointer m_rpcService;
    AtomicBoolean m_lastRequest;

    public:
    ChannelRPCServiceImpl(
        Channel::shared_pointer const & channel,
        ChannelRPCRequester::shared_pointer const & channelRPCRequester,
        RPCService::shared_pointer const & rpcService) :
        m_channel(channel),
        m_channelRPCRequester(channelRPCRequester),
        m_rpcService(rpcService),
        m_lastRequest()
    {
    }

    virtual ~ChannelRPCServiceImpl()
    {
        destroy();
    }

    void processRequest(epics::pvData::PVStructure::shared_pointer const & pvArgument)
    {
        epics::pvData::PVStructure::shared_pointer result;
        Status status = Status::Ok;
        bool ok = true;
        try
        {
            result = m_rpcService->request(pvArgument);
        }
        catch (RPCRequestException& rre)
        {
            status = Status(rre.getStatus(), rre.what());
            ok = false;
        }
        catch (std::exception& ex)
        {
            status = Status(Status::STATUSTYPE_FATAL, ex.what());
            ok = false;
        }
        catch (...)
        {
            // handle user unexpected errors
            status = Status(Status::STATUSTYPE_FATAL, "Unexpected exception caught while calling RPCService.request(PVStructure).");
            ok = false;
        }
    
        // check null result
        if (ok && result.get() == 0)
        {
            status = Status(Status::STATUSTYPE_FATAL, "RPCService.request(PVStructure) returned null.");
        }
        
        m_channelRPCRequester->requestDone(status, shared_from_this(), result);
        
        if (m_lastRequest.get())
            destroy();

    }

    virtual void request(epics::pvData::PVStructure::shared_pointer const & pvArgument)
    {
        processRequest(pvArgument);
    }

    void lastRequest()
    {
        m_lastRequest.set();
    }
    
    virtual Channel::shared_pointer getChannel()
    {
        return m_channel;
    }

    virtual void cancel()
    {
        // noop
    }

    virtual void destroy()
    {
        // noop
    }

    virtual void lock()
    {
        // noop
    }

    virtual void unlock()
    {
        // noop
    }
};




class RPCChannel :
    public Channel,
    public std::tr1::enable_shared_from_this<RPCChannel>
{
private:

    static Status notSupportedStatus;
    static Status destroyedStatus;
    
    AtomicBoolean m_destroyed;
    
    ChannelProvider::shared_pointer m_provider;
    string m_channelName;
    ChannelRequester::shared_pointer m_channelRequester;
    
    RPCService::shared_pointer m_rpcService;

public:    
    POINTER_DEFINITIONS(RPCChannel);
    
    RPCChannel(
           ChannelProvider::shared_pointer const & provider,
           string const & channelName,
           ChannelRequester::shared_pointer const & channelRequester,
           RPCService::shared_pointer const & rpcService) :
           m_provider(provider),
           m_channelName(channelName),
           m_channelRequester(channelRequester),
           m_rpcService(rpcService)
    {
    }

    virtual ~RPCChannel()
    {
        destroy();
    }

    virtual std::tr1::shared_ptr<ChannelProvider> getProvider()
    {
        return m_provider;
    }
    
    virtual std::string getRemoteAddress()
    {
        // local
        return getChannelName();
    }

    virtual ConnectionState getConnectionState()
    {
        return isConnected() ?
                Channel::CONNECTED :
                Channel::DESTROYED;
    }

    virtual std::string getChannelName()
    {
        return m_channelName;
    }

    virtual std::tr1::shared_ptr<ChannelRequester> getChannelRequester()
    {
        return m_channelRequester;
    }

    virtual bool isConnected()
    {
        return !m_destroyed.get();
    }


    virtual AccessRights getAccessRights(epics::pvData::PVField::shared_pointer const & /*pvField*/)
    {
        return none;
    }

    virtual void getField(GetFieldRequester::shared_pointer const & requester,std::string const & /*subField*/)
    {
        requester->getDone(notSupportedStatus, epics::pvData::Field::shared_pointer());    
    }

    virtual ChannelProcess::shared_pointer createChannelProcess(
            ChannelProcessRequester::shared_pointer const & channelProcessRequester,
            epics::pvData::PVStructure::shared_pointer const & /*pvRequest*/)
    {
        ChannelProcess::shared_pointer nullPtr;
        channelProcessRequester->channelProcessConnect(notSupportedStatus, nullPtr);
        return nullPtr; 
    }

    virtual ChannelGet::shared_pointer createChannelGet(
            ChannelGetRequester::shared_pointer const & channelGetRequester,
            epics::pvData::PVStructure::shared_pointer const & /*pvRequest*/)
    {
        ChannelGet::shared_pointer nullPtr;
        channelGetRequester->channelGetConnect(notSupportedStatus, nullPtr,
            epics::pvData::Structure::const_shared_pointer());
        return nullPtr; 
    }
            
    virtual ChannelPut::shared_pointer createChannelPut(
            ChannelPutRequester::shared_pointer const & channelPutRequester,
            epics::pvData::PVStructure::shared_pointer const & /*pvRequest*/)
    {
        ChannelPut::shared_pointer nullPtr;
        channelPutRequester->channelPutConnect(notSupportedStatus, nullPtr,
            epics::pvData::Structure::const_shared_pointer());
        return nullPtr; 
    }
            

    virtual ChannelPutGet::shared_pointer createChannelPutGet(
            ChannelPutGetRequester::shared_pointer const & channelPutGetRequester,
            epics::pvData::PVStructure::shared_pointer const & /*pvRequest*/)
    {
        ChannelPutGet::shared_pointer nullPtr;
        epics::pvData::Structure::const_shared_pointer nullStructure;
        channelPutGetRequester->channelPutGetConnect(notSupportedStatus, nullPtr, nullStructure, nullStructure);
        return nullPtr; 
    }

    virtual ChannelRPC::shared_pointer createChannelRPC(
            ChannelRPCRequester::shared_pointer const & channelRPCRequester,
            epics::pvData::PVStructure::shared_pointer const & /*pvRequest*/)
    {
        // nothing expected to be in pvRequest
        
        if (channelRPCRequester.get() == 0)
            throw std::invalid_argument("channelRPCRequester == null");

        if (m_destroyed.get())
        {
            ChannelRPC::shared_pointer nullPtr;
            channelRPCRequester->channelRPCConnect(destroyedStatus, nullPtr);
            return nullPtr;
        }
        
        // TODO use std::make_shared
        std::tr1::shared_ptr<ChannelRPCServiceImpl> tp(
            new ChannelRPCServiceImpl(shared_from_this(), channelRPCRequester, m_rpcService)
        );
        ChannelRPC::shared_pointer channelRPCImpl = tp;
        channelRPCRequester->channelRPCConnect(Status::Ok, channelRPCImpl);
        return channelRPCImpl;
    }

    virtual epics::pvData::Monitor::shared_pointer createMonitor(
            epics::pvData::MonitorRequester::shared_pointer const & monitorRequester,
            epics::pvData::PVStructure::shared_pointer const & /*pvRequest*/)
    {
        epics::pvData::Monitor::shared_pointer nullPtr;
        monitorRequester->monitorConnect(notSupportedStatus, nullPtr, epics::pvData::Structure::shared_pointer());
        return nullPtr; 
    }

    virtual ChannelArray::shared_pointer createChannelArray(
            ChannelArrayRequester::shared_pointer const & channelArrayRequester,
            epics::pvData::PVStructure::shared_pointer const & /*pvRequest*/)
    {
        ChannelArray::shared_pointer nullPtr;
        channelArrayRequester->channelArrayConnect(notSupportedStatus, nullPtr, epics::pvData::Array::const_shared_pointer());
        return nullPtr; 
    }
            

    virtual void printInfo()
    {
        printInfo(std::cout);
    }

    virtual void printInfo(std::ostream& out)
    {
        out << "RPCChannel: ";
        out << getChannelName();
        out << " [";
        out << Channel::ConnectionStateNames[getConnectionState()];
        out << "]";
    }

    virtual string getRequesterName()
    {
        return getChannelName();
    }
    
    virtual void message(std::string const & message,MessageType messageType)
    {
        // just delegate
        m_channelRequester->message(message, messageType);
    }

    virtual void destroy()
    {
        m_destroyed.set();   
    } 
};

Status RPCChannel::notSupportedStatus(Status::STATUSTYPE_ERROR, "only channelRPC requests are supported by this channel");
Status RPCChannel::destroyedStatus(Status::STATUSTYPE_ERROR, "channel destroyed");

Channel::shared_pointer createRPCChannel(ChannelProvider::shared_pointer const & provider,
                                          std::string const & channelName,
                                          ChannelRequester::shared_pointer const & channelRequester,
                                          RPCService::shared_pointer const & rpcService)
{
    // TODO use std::make_shared
    std::tr1::shared_ptr<RPCChannel> tp(
                new RPCChannel(provider, channelName, channelRequester, rpcService)
                );
    Channel::shared_pointer channel = tp;
    return channel;
}


class RPCChannelProvider :
    public virtual ChannelProvider, 
    public virtual ChannelFind,
    public std::tr1::enable_shared_from_this<RPCChannelProvider> {

public:
    POINTER_DEFINITIONS(RPCChannelProvider);
    
    static string PROVIDER_NAME;

    static Status noSuchChannelStatus;
    
    // TODO thread pool support
    
    RPCChannelProvider() {
    }

    virtual string getProviderName() {
        return PROVIDER_NAME;
    }

    virtual std::tr1::shared_ptr<ChannelProvider> getChannelProvider()
    {
        return shared_from_this();
    }
    
    virtual void cancel() {}

    virtual void destroy() {}
    
    virtual ChannelFind::shared_pointer channelFind(std::string const & channelName,
                        ChannelFindRequester::shared_pointer const & channelFindRequester)
    {
        bool found;
        {
            Lock guard(m_mutex);
            found = (m_services.find(channelName) != m_services.end());
        }
        ChannelFind::shared_pointer thisPtr(shared_from_this());
        channelFindRequester->channelFindResult(Status::Ok, thisPtr, found);
        return thisPtr;
    }


    virtual ChannelFind::shared_pointer channelList(
            ChannelListRequester::shared_pointer const & channelListRequester)
    {
        if (!channelListRequester.get())
            throw std::runtime_error("null requester");

        PVStringArray::svector channelNames;
        {
            Lock guard(m_mutex);
            channelNames.reserve(m_services.size());
            for (RPCServiceMap::const_iterator iter = m_services.begin();
                 iter != m_services.end();
                 iter++)
                channelNames.push_back(iter->first);
        }

        ChannelFind::shared_pointer thisPtr(shared_from_this());
        channelListRequester->channelListResult(Status::Ok, thisPtr, freeze(channelNames), false);
        return thisPtr;
    }

    virtual Channel::shared_pointer createChannel(
            std::string const & channelName,
            ChannelRequester::shared_pointer const & channelRequester,
            short /*priority*/)
    {
        RPCServiceMap::const_iterator iter;
        {
            Lock guard(m_mutex);
            iter = m_services.find(channelName);
        }
        
        if (iter == m_services.end())
        {
            Channel::shared_pointer nullChannel;
            channelRequester->channelCreated(noSuchChannelStatus, nullChannel);
            return nullChannel;
        }
               
        // TODO use std::make_shared
        std::tr1::shared_ptr<RPCChannel> tp(
            new RPCChannel(
                shared_from_this(),
                channelName,
                channelRequester,
                iter->second));
        Channel::shared_pointer rpcChannel = tp;
        channelRequester->channelCreated(Status::Ok, rpcChannel);
        return rpcChannel;
    }

    virtual Channel::shared_pointer createChannel(
        std::string const & /*channelName*/,
        ChannelRequester::shared_pointer const & /*channelRequester*/,
        short /*priority*/,
        std::string const & /*address*/)
    {
        // this will never get called by the pvAccess server
        throw std::runtime_error("not supported");
    }

    void registerService(std::string const & serviceName, RPCService::shared_pointer const & service)
    {
        Lock guard(m_mutex);
        m_services[serviceName] = service;
    }
    
    void unregisterService(std::string const & serviceName)
    {
        Lock guard(m_mutex);
        m_services.erase(serviceName);
    }

private:    
    typedef std::map<string, RPCService::shared_pointer> RPCServiceMap;
    RPCServiceMap m_services;
    epics::pvData::Mutex m_mutex;
};

string RPCChannelProvider::PROVIDER_NAME("rpcService");
Status RPCChannelProvider::noSuchChannelStatus(Status::STATUSTYPE_ERROR, "no such channel");



class RPCChannelProviderFactory : public ChannelProviderFactory
{
public:
    POINTER_DEFINITIONS(RPCChannelProviderFactory);

    RPCChannelProviderFactory() :
        m_channelProviderImpl(new RPCChannelProvider())
    {
    }

    virtual std::string getFactoryName()
    {
        return RPCChannelProvider::PROVIDER_NAME;
    }

    virtual ChannelProvider::shared_pointer sharedInstance()
    {
        return m_channelProviderImpl;
    }

    virtual ChannelProvider::shared_pointer newInstance()
    {
        // TODO use std::make_shared
        std::tr1::shared_ptr<RPCChannelProvider> tp(new RPCChannelProvider());
        ChannelProvider::shared_pointer channelProvider = tp;
        return channelProvider;
    }

private:
    RPCChannelProvider::shared_pointer m_channelProviderImpl;
};


RPCServer::RPCServer()
{
    // TODO factory is never deregistered, multiple RPCServer instances create multiple factories, etc.
    m_channelProviderFactory.reset(new RPCChannelProviderFactory());
    registerChannelProviderFactory(m_channelProviderFactory);

    m_channelProviderImpl = m_channelProviderFactory->sharedInstance();

    m_serverContext = ServerContextImpl::create();
    m_serverContext->setChannelProviderName(m_channelProviderImpl->getProviderName());
 
    m_serverContext->initialize(getChannelProviderRegistry());
}

RPCServer::~RPCServer()
{
    // multiple destroy call is OK
    destroy();
}

void RPCServer::printInfo()
{
    std::cout << m_serverContext->getVersion().getVersionString() << std::endl;
    m_serverContext->printInfo();
}

void RPCServer::run(int seconds)
{
    m_serverContext->run(seconds);
}

struct ThreadRunnerParam {
    RPCServer::shared_pointer server;
    int timeToRun;
};

static void threadRunner(void* usr)
{
    ThreadRunnerParam* pusr = static_cast<ThreadRunnerParam*>(usr);
    ThreadRunnerParam param = *pusr;
    delete pusr;

    param.server->run(param.timeToRun);
}

/// Method requires usage of std::tr1::shared_ptr<RPCServer>. This instance must be 
/// owned by a shared_ptr instance.
void RPCServer::runInNewThread(int seconds)
{
    std::auto_ptr<ThreadRunnerParam> param(new ThreadRunnerParam());
    param->server = shared_from_this();
    param->timeToRun = seconds;

    epicsThreadCreate("RPCServer thread",
                      epicsThreadPriorityMedium,
                      epicsThreadGetStackSize(epicsThreadStackSmall),
                      threadRunner, param.get());

    // let the thread delete 'param'
    param.release();
}

void RPCServer::destroy()
{
    m_serverContext->destroy();
}

void RPCServer::registerService(std::string const & serviceName, RPCService::shared_pointer const & service)
{
    std::tr1::dynamic_pointer_cast<RPCChannelProvider>(m_channelProviderImpl)->registerService(serviceName, service);
}

void RPCServer::unregisterService(std::string const & serviceName)
{
    std::tr1::dynamic_pointer_cast<RPCChannelProvider>(m_channelProviderImpl)->unregisterService(serviceName);
}

}}
