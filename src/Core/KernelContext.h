//////////////////////////////////////////////////////////////////////////////
// Filename    : KernelContext.h
// Description : The process-wide managers whose classes live in src/Core,
//               handed to a caller explicitly instead of looked up through a
//               global.
//
//               These three are server-agnostic: every one of the three
//               binaries creates a set of its own, so they belong here rather
//               than on any one server's context. A Core translation unit may
//               reach them too, which a header under src/server/ could never
//               offer it (rule K1).
//
//               The context does NOT own the managers: each is still created
//               and destroyed by the startup code that holds it (each
//               server's application for the configuration, its server object for
//               the packet factory table and the validator), and is registered
//               when its owning startup stage completes. A manager is null
//               until its publication point is reached, and an accessor asserts
//               on a null one: reading a manager before it exists is a
//               startup-order bug, not a runtime condition to branch on.
//
//               Only forward declarations live here, so the header costs a
//               caller nothing and can be included where none of the managers
//               are linked.
//////////////////////////////////////////////////////////////////////////////

#ifndef __KERNEL_CONTEXT_H__
#define __KERNEL_CONTEXT_H__

class PacketFactoryManager;
class PacketValidator;
class Properties;

namespace de {

class KernelContext {
public:
    KernelContext() = default;

    KernelContext(const KernelContext&) = delete;
    KernelContext& operator=(const KernelContext&) = delete;

    void setConfig(Properties* pConfig) {
        m_pConfig = pConfig;
    }
    // Return the previous binding so a scoped configuration owner can restore
    // it, including the initially empty context, without a nullable accessor.
    Properties* exchangeConfig(Properties* pConfig) noexcept {
        Properties* previous = m_pConfig;
        m_pConfig = pConfig;
        return previous;
    }
    void setPacketFactoryManager(PacketFactoryManager* pPacketFactoryManager) {
        m_pPacketFactoryManager = pPacketFactoryManager;
    }
    PacketFactoryManager* exchangePacketFactoryManager(PacketFactoryManager* manager) noexcept {
        PacketFactoryManager* previous = m_pPacketFactoryManager;
        m_pPacketFactoryManager = manager;
        return previous;
    }
    void setPacketValidator(PacketValidator* pPacketValidator) {
        m_pPacketValidator = pPacketValidator;
    }
    PacketValidator* exchangePacketValidator(PacketValidator* validator) noexcept {
        PacketValidator* previous = m_pPacketValidator;
        m_pPacketValidator = validator;
        return previous;
    }

    Properties& config() const;
    PacketFactoryManager& packetFactories() const;
    PacketValidator& packetValidator() const;

private:
    Properties* m_pConfig = nullptr;
    PacketFactoryManager* m_pPacketFactoryManager = nullptr;
    PacketValidator* m_pPacketValidator = nullptr;
};

// The process-wide context the startup code fills. A converted subsystem is
// handed the context and never calls this; the call belongs at the boundary
// where a subsystem is created from code that still reads globals.
KernelContext& kernelContext();

} // namespace de

#endif
