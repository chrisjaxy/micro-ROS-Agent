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

#include <gattlib.h>
#include <glib.h>
#include <signal.h>
#include <mqueue.h>

typedef enum { READ, WRITE} operation_t;
operation_t g_operation;

static uuid_t g_uuid;
static uuid_t g_notify_uuid;

char write_uuid[]        = "0000fff2-0000-1000-8000-00805f9b34fb";
char notification_uuid[] = "0000fff1-0000-1000-8000-00805f9b34fb";

static GMainLoop *m_main_loop;

mqd_t receive_mq;

static void on_user_abort(int arg) {
    (void) arg;
	g_main_loop_quit(m_main_loop);
}

void notification_handler(const uuid_t* uuid, const uint8_t* data, size_t data_length, void* user_data) {
	(void)uuid;
    (void)user_data;

    mq_send(receive_mq, (char *)data, data_length, 0);
}

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

    if (!strcmp("ble", argv[1]) && argc > 2) {
        result       = true;
        custom_agent = true;
    } else {
        result = xrce_dds_agent_instance_.create(argc, argv);
    }

    if (result) {
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

    if (custom_agent) {       
        eprosima::uxr::Middleware::Kind mw_kind(eprosima::uxr::Middleware::Kind::FASTDDS);
        gatt_connection_t* connection;

        if (gattlib_string_to_uuid(write_uuid, strlen(write_uuid) + 1, &g_uuid) < 0) {
            printf("write_uuid %s error \n", write_uuid);
            return 1;
        }

        if (gattlib_string_to_uuid(notification_uuid, strlen(notification_uuid) + 1, &g_notify_uuid) < 0) {
            printf("notification_uuid %s error \n", notification_uuid);
            return 1;
        }

        struct mq_attr attr;
        attr.mq_flags   = O_NONBLOCK;
        attr.mq_msgsize = 1024;
        attr.mq_maxmsg  = 10;
        
        receive_mq = mq_open("/ble", O_RDWR | O_CREAT, 0666, NULL);
        if (receive_mq == -1) {
            printf("mq_open errno %d\n", errno);
            return 1;
        }
        mq_setattr(receive_mq, &attr, NULL);  // 设置为非阻塞

        /**
         * @brief Agent's initialization behaviour description.
         */
        eprosima::uxr::CustomAgent::InitFunction init_function = [&]() -> bool
        {
            connection = gattlib_connect(NULL, argv[2], GATTLIB_CONNECTION_OPTIONS_LEGACY_DEFAULT);
            if (connection == NULL) {
                printf("Fail to connect to the bluetooth device %s \n", argv[2]);
                return false;
            }

            gattlib_register_notification(connection, notification_handler, NULL);

            if (gattlib_notification_start(connection, &g_notify_uuid)) {
                printf("Fail to start notification.");
                return false;
            }

            return true;
        };

        /**
         * @brief Agent's destruction actions.
         */
        eprosima::uxr::CustomAgent::FiniFunction fini_function = [&]() -> bool
        {
            if (connection != NULL) {
                gattlib_notification_stop(connection, &g_notify_uuid);
                gattlib_disconnect(connection);
            }

            return true;
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
            (void)source_endpoint;
            (void)buffer;
            (void)buffer_length;
            (void)timeout;

            int bytes_received = mq_receive(receive_mq, (char *)buffer, buffer_length, NULL);

            if (bytes_received > 0) {
                transport_rc = eprosima::uxr::TransportRc::ok;
                return (size_t) bytes_received;
            } else {
                transport_rc = eprosima::uxr::TransportRc::timeout_error;
                return 0;
            }
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
            (void)destination_endpoint;
            if (gattlib_write_char_by_uuid(connection, &g_uuid, buffer, message_length) != 0) {
                printf("Error while writing \n");
            }
            transport_rc = eprosima::uxr::TransportRc::ok;
            return message_length;
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

            /**
             * Create a custom agent instance.
             */
            eprosima::uxr::CustomAgent custom_agent(
                "BLE_CUSTOM",
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
             * Run agent
             */
            custom_agent.start();
            
            signal(SIGINT, on_user_abort);

            m_main_loop = g_main_loop_new(NULL, 0);
            g_main_loop_run(m_main_loop);
	        g_main_loop_unref(m_main_loop);

            mq_close(receive_mq);

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
    } else {
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