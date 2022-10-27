//include stuff here
#include <cstdio>

#include <mutex>
#include <condition_variable>
#include <grpc/grpc.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/health_check_service_interface.h>

#include "AglShellGrpcClient.h"
#include "agl_shell.grpc.pb.h"

namespace {
	const char kDefaultGrpcServiceAddress[] = "127.0.0.1:14005";
}

GrpcClient::GrpcClient()
{
	auto channel = grpc::CreateChannel(kDefaultGrpcServiceAddress,
			grpc::InsecureChannelCredentials());

	// init the stub here
	m_stub = agl_shell_ipc::AglShellManagerService::NewStub(channel);
	reader = new Reader(m_stub.get());
}


bool
GrpcClient::ActivateApp(const std::string& app_id, const std::string& output_name)
{
	agl_shell_ipc::ActivateRequest request;

	request.set_app_id(app_id);
	request.set_output_name(output_name);

	grpc::ClientContext context;
	::agl_shell_ipc::ActivateResponse reply;

	grpc::Status status = m_stub->ActivateApp(&context, request, &reply);
	return status.ok();
}

bool
GrpcClient::DeactivateApp(const std::string& app_id)
{
	agl_shell_ipc::DeactivateRequest request;

	request.set_app_id(app_id);

	grpc::ClientContext context;
	::agl_shell_ipc::DeactivateResponse reply;

	grpc::Status status = m_stub->DeactivateApp(&context, request, &reply);
	return status.ok();
}

grpc::Status
GrpcClient::Wait(void)
{
	return reader->Await();
}

void
GrpcClient::AppStatusState(Callback callback)
{
	reader->AppStatusState(callback);
}
