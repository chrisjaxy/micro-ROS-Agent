// Copyright 2020 Proyectos y Sistemas de Mantenimiento SL (eProsima).
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef _UROS_AGENT_AGENT_CPP
#define _UROS_AGENT_AGENT_CPP

#include <agent/Agent.hpp>

#include <utility>
#include <memory>

#include <uxr/agent/transport/custom/CustomAgent.hpp>
#include <uxr/agent/transport/endpoint/IPv4EndPoint.hpp>

#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
namespace uros {
namespace agent {

Agent::Agent()
    : xrce_dds_agent_instance_(xrce_dds_agent_instance_.getInstance())
{
}

bool Agent::create(
        int argc,
        char** argv)
{
    bool result       = false;
    bool custom_agent = false;

    if (!strcmp("custom", argv[1]) && argc > 3)
    {
        result       = true;
        custom_agent = true;
    }
    else
    {
        result = xrce_dds_agent_instance_.create(argc, argv);
    }

    if (result)
    {
        /**
         * Add CREATE_PARTICIPANT callback.
         */
        std::function<void (
            const eprosima::fastdds::dds::DomainParticipant *)> on_create_participant
            ([&](
                const eprosima::fastdds::dds::DomainParticipant* participant) -> void
            {
                auto graph_manager_ = find_or_create_graph_manager(participant->get_domain_id());
                graph_manager_->add_participant(participant);
            });
        xrce_dds_agent_instance_.add_middleware_callback(
            eprosima::uxr::Middleware::Kind::FASTDDS,
            eprosima::uxr::middleware::CallbackKind::CREATE_PARTICIPANT,
            std::move(on_create_participant));

        /**
         * Add REMOVE_PARTICIPANT callback.
         */
        std::function<void (
            const eprosima::fastdds::dds::DomainParticipant *)> on_delete_participant
            ([&](
                const eprosima::fastdds::dds::DomainParticipant* participant) -> void
            {
                auto graph_manager_ = find_or_create_graph_manager(participant->get_domain_id());
                graph_manager_->remove_participant(participant);
            });
        xrce_dds_agent_instance_.add_middleware_callback(
            eprosima::uxr::Middleware::Kind::FASTDDS,
            eprosima::uxr::middleware::CallbackKind::DELETE_PARTICIPANT,
            std::move(on_delete_participant));

        /**
         * Add CREATE_DATAWRITER callback.
         */
        std::function<void (
            const eprosima::fastdds::dds::DomainParticipant *,
            const eprosima::fastdds::dds::DataWriter *)> on_create_datawriter
            ([&](
                const eprosima::fastdds::dds::DomainParticipant* participant,
                const eprosima::fastdds::dds::DataWriter* datawriter) -> void
            {
                auto graph_manager_ = find_or_create_graph_manager(participant->get_domain_id());

                // TODO(jamoralp): Workaround for Fast-DDS bug #9977. Remove when fixed
                const eprosima::fastrtps::rtps::InstanceHandle_t instance_handle =
                    datawriter->get_instance_handle();
                const eprosima::fastrtps::rtps::GUID_t datawriter_guid =
                    iHandle2GUID(instance_handle);
                graph_manager_->add_datawriter(datawriter_guid, participant, datawriter);
                graph_manager_->associate_entity(
                    datawriter_guid, participant, dds::xrce::OBJK_DATAWRITER);
            });
        xrce_dds_agent_instance_.add_middleware_callback(
            eprosima::uxr::Middleware::Kind::FASTDDS,
            eprosima::uxr::middleware::CallbackKind::CREATE_DATAWRITER,
            std::move(on_create_datawriter));

        /**
         * Add DELETE_DATAWRITER callback.
         */
        std::function<void (
            const eprosima::fastdds::dds::DomainParticipant *,
            const eprosima::fastdds::dds::DataWriter *)> on_delete_datawriter
            ([&](
                const eprosima::fastdds::dds::DomainParticipant* participant,
                const eprosima::fastdds::dds::DataWriter* datawriter) -> void
            {

                auto graph_manager_ = find_or_create_graph_manager(participant->get_domain_id());

                // TODO(jamoralp): Workaround for Fast-DDS bug #9977. Remove when fixed
                const eprosima::fastrtps::rtps::InstanceHandle_t instance_handle =
                    datawriter->get_instance_handle();
                const eprosima::fastrtps::rtps::GUID_t datawriter_guid =
                    eprosima::fastrtps::rtps::iHandle2GUID(instance_handle);
                graph_manager_->remove_datawriter(datawriter_guid);
            });

        xrce_dds_agent_instance_.add_middleware_callback(
            eprosima::uxr::Middleware::Kind::FASTDDS,
            eprosima::uxr::middleware::CallbackKind::DELETE_DATAWRITER,
            std::move(on_delete_datawriter));

        /**
         * Add CREATE_DATAREADER callback.
         */
        std::function<void (
            const eprosima::fastdds::dds::DomainParticipant *,
            const eprosima::fastdds::dds::DataReader*)> on_create_datareader
            ([&](
                const eprosima::fastdds::dds::DomainParticipant* participant,
                const eprosima::fastdds::dds::DataReader* datareader) -> void
            {
                auto graph_manager_ = find_or_create_graph_manager(participant->get_domain_id());

                // TODO(jamoralp): Workaround for Fast-DDS bug #9977. Remove when fixed
                const eprosima::fastrtps::rtps::InstanceHandle_t instance_handle =
                    datareader->get_instance_handle();
                const eprosima::fastrtps::rtps::GUID_t datareader_guid =
                    eprosima::fastrtps::rtps::iHandle2GUID(instance_handle);
                graph_manager_->add_datareader(datareader_guid, participant, datareader);
                graph_manager_->associate_entity(
                    datareader_guid, participant, dds::xrce::OBJK_DATAREADER);
            });
        xrce_dds_agent_instance_.add_middleware_callback(
            eprosima::uxr::Middleware::Kind::FASTDDS,
            eprosima::uxr::middleware::CallbackKind::CREATE_DATAREADER,
            std::move(on_create_datareader));

        /**
         * Add DELETE_DATAREADER callback.
         */
        std::function<void (
            const eprosima::fastdds::dds::DomainParticipant *,
            const eprosima::fastdds::dds::DataReader *)> on_delete_datareader
            ([&](
                const eprosima::fastdds::dds::DomainParticipant* participant,
                const eprosima::fastdds::dds::DataReader* datareader) -> void
            {
                auto graph_manager_ = find_or_create_graph_manager(participant->get_domain_id());

                // TODO(jamoralp): Workaround for Fast-DDS bug #9977. Remove when fixed
                const eprosima::fastrtps::rtps::InstanceHandle_t instance_handle =
                    datareader->get_instance_handle();
                const eprosima::fastrtps::rtps::GUID_t datareader_guid =
                    eprosima::fastrtps::rtps::iHandle2GUID(instance_handle);
                graph_manager_->remove_datareader(datareader_guid);
            });

        xrce_dds_agent_instance_.add_middleware_callback(
            eprosima::uxr::Middleware::Kind::FASTDDS,
            eprosima::uxr::middleware::CallbackKind::DELETE_DATAREADER,
            std::move(on_delete_datareader));

        /**
         * Add CREATE_REQUESTER callback.
         */
        std::function<void (
            const eprosima::fastdds::dds::DomainParticipant *,
            const eprosima::fastdds::dds::DataWriter *,
            const eprosima::fastdds::dds::DataReader *)> on_create_requester
            ([&](
                const eprosima::fastdds::dds::DomainParticipant* participant,
                const eprosima::fastdds::dds::DataWriter* datawriter,
                const eprosima::fastdds::dds::DataReader * datareader) -> void
            {
                auto graph_manager_ = find_or_create_graph_manager(participant->get_domain_id());

                // TODO(pablogs): Workaround for Fast-DDS bug #9977. Remove when fixed
                const eprosima::fastrtps::rtps::InstanceHandle_t instance_handle_dw =
                    datawriter->get_instance_handle();
                const eprosima::fastrtps::rtps::GUID_t datawriter_guid =
                    eprosima::fastrtps::rtps::iHandle2GUID(instance_handle_dw);

                graph_manager_->add_datawriter(datawriter_guid, participant, datawriter);
                graph_manager_->associate_entity(
                    datawriter_guid, participant, dds::xrce::OBJK_DATAWRITER);

                // TODO(pablogs): Workaround for Fast-DDS bug #9977. Remove when fixed
                const eprosima::fastrtps::rtps::InstanceHandle_t instance_handle_dr =
                    datareader->get_instance_handle();
                const eprosima::fastrtps::rtps::GUID_t datareader_guid =
                    eprosima::fastrtps::rtps::iHandle2GUID(instance_handle_dr);
                graph_manager_->add_datareader(datareader_guid, participant, datareader);
                graph_manager_->associate_entity(
                    datareader_guid, participant, dds::xrce::OBJK_DATAREADER);
            });
        xrce_dds_agent_instance_.add_middleware_callback(
            eprosima::uxr::Middleware::Kind::FASTDDS,
            eprosima::uxr::middleware::CallbackKind::CREATE_REQUESTER,
            std::move(on_create_requester));

        /**
         * Add DELETE_REQUESTER callback.
         */
        std::function<void (
            const eprosima::fastdds::dds::DomainParticipant *,
            const eprosima::fastdds::dds::DataWriter *,
            const eprosima::fastdds::dds::DataReader *)> on_delete_requester
            ([&](
                const eprosima::fastdds::dds::DomainParticipant* participant,
                const eprosima::fastdds::dds::DataWriter* datawriter,
                const eprosima::fastdds::dds::DataReader * datareader) -> void
            {
                auto graph_manager_ = find_or_create_graph_manager(participant->get_domain_id());

                // TODO(pablogs): Workaround for Fast-DDS bug #9977. Remove when fixed
                const eprosima::fastrtps::rtps::InstanceHandle_t instance_handle_dw =
                    datawriter->get_instance_handle();
                const eprosima::fastrtps::rtps::GUID_t datawriter_guid =
                    eprosima::fastrtps::rtps::iHandle2GUID(instance_handle_dw);
                graph_manager_->remove_datawriter(datawriter_guid);

                // TODO(pablogs): Workaround for Fast-DDS bug #9977. Remove when fixed
                const eprosima::fastrtps::rtps::InstanceHandle_t instance_handle_dr =
                    datareader->get_instance_handle();
                const eprosima::fastrtps::rtps::GUID_t datareader_guid =
                    eprosima::fastrtps::rtps::iHandle2GUID(instance_handle_dr);
                graph_manager_->remove_datareader(datareader_guid);
            });

        xrce_dds_agent_instance_.add_middleware_callback(
            eprosima::uxr::Middleware::Kind::FASTDDS,
            eprosima::uxr::middleware::CallbackKind::DELETE_REQUESTER,
            std::move(on_delete_requester));

        /**
         * Add CREATE_REPLIER callback.
         */
        std::function<void (
            const eprosima::fastdds::dds::DomainParticipant *,
            const eprosima::fastdds::dds::DataWriter *,
            const eprosima::fastdds::dds::DataReader *)> on_create_replier
            ([&](
                const eprosima::fastdds::dds::DomainParticipant* participant,
                const eprosima::fastdds::dds::DataWriter* datawriter,
                const eprosima::fastdds::dds::DataReader * datareader) -> void
            {
                auto graph_manager_ = find_or_create_graph_manager(participant->get_domain_id());

                // TODO(pablogs): Workaround for Fast-DDS bug #9977. Remove when fixed
                const eprosima::fastrtps::rtps::InstanceHandle_t instance_handle_dw =
                    datawriter->get_instance_handle();
                const eprosima::fastrtps::rtps::GUID_t datawriter_guid =
                    eprosima::fastrtps::rtps::iHandle2GUID(instance_handle_dw);
                graph_manager_->add_datawriter(datawriter_guid, participant, datawriter);
                graph_manager_->associate_entity(
                    datawriter_guid, participant, dds::xrce::OBJK_DATAWRITER);

                // TODO(pablogs): Workaround for Fast-DDS bug #9977. Remove when fixed
                const eprosima::fastrtps::rtps::InstanceHandle_t instance_handle_dr =
                    datareader->get_instance_handle();
                const eprosima::fastrtps::rtps::GUID_t datareader_guid =
                    eprosima::fastrtps::rtps::iHandle2GUID(instance_handle_dr);
                graph_manager_->add_datareader(datareader_guid, participant, datareader);
                graph_manager_->associate_entity(
                    datareader_guid, participant, dds::xrce::OBJK_DATAREADER);
            });
        xrce_dds_agent_instance_.add_middleware_callback(
            eprosima::uxr::Middleware::Kind::FASTDDS,
            eprosima::uxr::middleware::CallbackKind::CREATE_REPLIER,
            std::move(on_create_replier));

        /**
         * Add DELETE_REPLIER callback.
         */
        std::function<void (
            const eprosima::fastdds::dds::DomainParticipant *,
            const eprosima::fastdds::dds::DataWriter *,
            const eprosima::fastdds::dds::DataReader *)> on_delete_replier
            ([&](
                const eprosima::fastdds::dds::DomainParticipant* participant,
                const eprosima::fastdds::dds::DataWriter* datawriter,
                const eprosima::fastdds::dds::DataReader * datareader) -> void
            {
                auto graph_manager_ = find_or_create_graph_manager(participant->get_domain_id());

                // TODO(pablogs): Workaround for Fast-DDS bug #9977. Remove when fixed
                const eprosima::fastrtps::rtps::InstanceHandle_t instance_handle_dw =
                    datawriter->get_instance_handle();
                const eprosima::fastrtps::rtps::GUID_t datawriter_guid =
                    eprosima::fastrtps::rtps::iHandle2GUID(instance_handle_dw);
                graph_manager_->remove_datawriter(datawriter_guid);

                // TODO(pablogs): Workaround for Fast-DDS bug #9977. Remove when fixed
                const eprosima::fastrtps::rtps::InstanceHandle_t instance_handle_dr =
                    datareader->get_instance_handle();
                const eprosima::fastrtps::rtps::GUID_t datareader_guid =
                    eprosima::fastrtps::rtps::iHandle2GUID(instance_handle_dr);
                graph_manager_->remove_datareader(datareader_guid);
            });

        xrce_dds_agent_instance_.add_middleware_callback(
            eprosima::uxr::Middleware::Kind::FASTDDS,
            eprosima::uxr::middleware::CallbackKind::DELETE_REPLIER,
            std::move(on_delete_replier));
    }

    if (custom_agent)
    {       
        eprosima::uxr::Middleware::Kind mw_kind(eprosima::uxr::Middleware::Kind::FASTDDS);
        uint16_t agent_port(std::stoi(argv[3]));
        struct pollfd poll_fd;

        /**
         * @brief Agent's initialization behaviour description.
         */
        eprosima::uxr::CustomAgent::InitFunction init_function = [&]() -> bool
        {
            bool rv = false;
            poll_fd.fd = socket(PF_INET, SOCK_DGRAM, 0);

            if (-1 != poll_fd.fd)
            {
                struct sockaddr_in address{};

                address.sin_family = AF_INET;
                address.sin_port = htons(agent_port);
                address.sin_addr.s_addr = INADDR_ANY;
                memset(address.sin_zero, '\0', sizeof(address.sin_zero));

                if (-1 != bind(poll_fd.fd,
                            reinterpret_cast<struct sockaddr*>(&address),
                            sizeof(address)))
                {
                    poll_fd.events = POLLIN;
                    rv = true;

                    UXR_AGENT_LOG_INFO(
                        UXR_DECORATE_GREEN(
                            "This is an example of a custom Micro XRCE-DDS Agent INIT function"),
                        "port: {}",
                        agent_port);
                }
            }

            return rv;
        };

        /**
         * @brief Agent's destruction actions.
         */
        eprosima::uxr::CustomAgent::FiniFunction fini_function = [&]() -> bool
        {
            if (-1 == poll_fd.fd)
            {
                return true;
            }

            if (0 == ::close(poll_fd.fd))
            {
                poll_fd.fd = -1;

                UXR_AGENT_LOG_INFO(
                    UXR_DECORATE_GREEN(
                        "This is an example of a custom Micro XRCE-DDS Agent FINI function"),
                    "port: {}",
                    agent_port);

                return true;
            }
            else
            {
                return false;
            }
        };

        /**
         * @brief Agent's incoming data functionality.
         */
        eprosima::uxr::CustomAgent::RecvMsgFunction recv_msg_function = [&](
                eprosima::uxr::CustomEndPoint* source_endpoint,
                uint8_t* buffer,
                size_t buffer_length,
                int timeout,
                eprosima::uxr::TransportRc& transport_rc) -> ssize_t
        {
            struct sockaddr_in client_addr{};
            socklen_t client_addr_len = sizeof(struct sockaddr_in);
            ssize_t bytes_received = -1;

            int poll_rv = poll(&poll_fd, 1, timeout);

            if (0 < poll_rv)
            {
                bytes_received = recvfrom(
                    poll_fd.fd,
                    buffer,
                    buffer_length,
                    0,
                    reinterpret_cast<struct sockaddr *>(&client_addr),
                    &client_addr_len);

                transport_rc = (-1 != bytes_received)
                    ? eprosima::uxr::TransportRc::ok
                    : eprosima::uxr::TransportRc::server_error;
            }
            else
            {
                transport_rc = (0 == poll_rv)
                    ? eprosima::uxr::TransportRc::timeout_error
                    : eprosima::uxr::TransportRc::server_error;
                bytes_received = 0;
            }

            if (eprosima::uxr::TransportRc::ok == transport_rc)
            {
                UXR_AGENT_LOG_INFO(
                    UXR_DECORATE_GREEN(
                        "This is an example of a custom Micro XRCE-DDS Agent RECV_MSG function"),
                    "port: {}",
                    agent_port);
                source_endpoint->set_member_value<uint32_t>("address",
                    static_cast<uint32_t>(client_addr.sin_addr.s_addr));
                source_endpoint->set_member_value<uint16_t>("port",
                    static_cast<uint16_t>(client_addr.sin_port));
            }


            return bytes_received;
        };

        /**
         * @brief Agent's outcoming data flow definition.
         */
        eprosima::uxr::CustomAgent::SendMsgFunction send_msg_function = [&](
            const eprosima::uxr::CustomEndPoint* destination_endpoint,
            uint8_t* buffer,
            size_t message_length,
            eprosima::uxr::TransportRc& transport_rc) -> ssize_t
        {
            struct sockaddr_in client_addr{};

            memset(&client_addr, 0, sizeof(client_addr));
            client_addr.sin_family = AF_INET;
            client_addr.sin_port = destination_endpoint->get_member<uint16_t>("port");
            client_addr.sin_addr.s_addr = destination_endpoint->get_member<uint32_t>("address");

            ssize_t bytes_sent =
                sendto(
                    poll_fd.fd,
                    buffer,
                    message_length,
                    0,
                    reinterpret_cast<struct sockaddr*>(&client_addr),
                    sizeof(client_addr));

            transport_rc = (-1 != bytes_sent)
                ? eprosima::uxr::TransportRc::ok
                : eprosima::uxr::TransportRc::server_error;

            if (eprosima::uxr::TransportRc::ok == transport_rc)
            {
                UXR_AGENT_LOG_INFO(
                    UXR_DECORATE_GREEN(
                        "This is an example of a custom Micro XRCE-DDS Agent SEND_MSG function"),
                    "port: {}",
                    agent_port);
            }

            return bytes_sent;
        };

        /**
         * Run the main application.
         */
        try
        {
            /**
             * EndPoint definition for this transport. We define an address and a port.
             */
            eprosima::uxr::CustomEndPoint custom_endpoint;
            custom_endpoint.add_member<uint32_t>("address");
            custom_endpoint.add_member<uint16_t>("port");

            /**
             * Create a custom agent instance.
             */
            eprosima::uxr::CustomAgent custom_agent(
                "UDPv4_CUSTOM",
                &custom_endpoint,
                mw_kind,
                false,
                init_function,
                fini_function,
                send_msg_function,
                recv_msg_function);

            /**
             * Set verbosity level
             */
            custom_agent.set_verbose_level(6);

            /**
             * Run agent and wait until receiving an stop signal.
             */
            custom_agent.start();

            int n_signal = 0;
            sigset_t signals;
            sigwait(&signals, &n_signal);

            /**
             * Stop agent, and exit.
             */
            custom_agent.stop();
            return 0;
        }
        catch (const std::exception& e)
        {
            std::cout << e.what() << std::endl;
            return 1;
        }
    }

    return result;
}

void Agent::run()
{
    return xrce_dds_agent_instance_.run();
}

std::shared_ptr<graph_manager::GraphManager> Agent::find_or_create_graph_manager(eprosima::fastdds::dds::DomainId_t domain_id)
{

auto it = graph_manager_map_.find(domain_id);

    if (it != graph_manager_map_.end()) {
        return it->second;
    }else{
        return graph_manager_map_.insert(
            std::make_pair(
                domain_id,
                std::make_shared<graph_manager::GraphManager>(domain_id)
            )
        ).first->second;
    }
}

}  // namespace agent
}  // namespace uros
#endif  // _UROS_AGENT_AGENT_CPP