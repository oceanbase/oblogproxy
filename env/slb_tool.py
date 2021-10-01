import json
import logging

from aliyunsdkcore.client import AcsClient
from aliyunsdkcore.request import CommonRequest
from aliyunsdkslb.request.v20140515.CreateLoadBalancerRequest import CreateLoadBalancerRequest
from aliyunsdkslb.request.v20140515.CreateLoadBalancerTCPListenerRequest import CreateLoadBalancerTCPListenerRequest
from aliyunsdkslb.request.v20140515.CreateMasterSlaveServerGroupRequest import CreateMasterSlaveServerGroupRequest
from aliyunsdkslb.request.v20140515.DeleteLoadBalancerListenerRequest import DeleteLoadBalancerListenerRequest
from aliyunsdkslb.request.v20140515.DeleteLoadBalancerRequest import DeleteLoadBalancerRequest
from aliyunsdkslb.request.v20140515.DeleteMasterSlaveServerGroupRequest import DeleteMasterSlaveServerGroupRequest
from aliyunsdkslb.request.v20140515.DescribeLoadBalancerAttributeRequest import DescribeLoadBalancerAttributeRequest
from aliyunsdkslb.request.v20140515.StartLoadBalancerListenerRequest import StartLoadBalancerListenerRequest
from aliyunsdkslb.request.v20140515.StopLoadBalancerListenerRequest import StopLoadBalancerListenerRequest
from aliyunsdkslb.request.v20140515.CreateVServerGroupRequest import CreateVServerGroupRequest

logger = logging.getLogger("worker")


class Slb:

    def __init__(self, region_no, accessKeyId, securityToken):
        self.region_no = region_no
        self.api_api_key = accessKeyId
        self.api_secret_key = securityToken

        self.client = AcsClient(accessKeyId, securityToken, region_no)

    def __do_action(self, request):
        logger.info(request.get_query_params())
        response = self.client.do_action_with_exception(request)
        logger.info(json.loads(response.decode()))
        return json.loads(response.decode())

    def modify_bypass_toa_attribute(self, instance_id, instance_type):
        query_params = {'InstanceId': instance_id,
                        'InstanceType': instance_type,
                        'BypassToa': 1
                        }
        request = CommonRequest()
        request.set_action_name('ModifyBypassToaAttribute')
        request.set_version('2016-04-28')
        request.set_domain('vpc.aliyuncs.com')
        request.set_query_params(query_params)
        response = self.__do_action(request)
        return response

    def create_single_tunnel_load_balance(self, load_balance_spec, master_zone_id, slave_zone_id, vpc_id, vswitch_id,
                                          load_balance_name, uid, resource_id):
        query_params = {'RegionId': self.region_no,
                        'LoadBalancerName': load_balance_name,
                        'LoadBalancerSpec': load_balance_spec,
                        'MasterZoneId': master_zone_id,
                        'SlaveZoneId': slave_zone_id,
                        'VSwitchId': vswitch_id,
                        'VpcId': vpc_id,
                        'CloudInstanceType': 'aliyun_oceanbase',
                        'CloudInstanceUid': uid,
                        'CloudInstanceId': load_balance_name,
                        'TunnelType': 'singleTunnel',
                        'ResourceId': resource_id}
        request = CommonRequest()
        request.set_action_name('CreateLoadBalancerForCloudService')
        request.set_domain('slb.aliyuncs.com')
        request.set_version('2014-05-15')
        request.set_query_params(query_params)
        response = self.__do_action(request)
        return response

    def create_master_slave_group(self, load_balance_id, group_name, master_server_id, slave_server_id,
                                  port):
        request = CreateMasterSlaveServerGroupRequest()
        load_balance_attribute = self.describe_load_balance_attribute(load_balance_id)
        request.set_accept_format('json')

        servers = [
            {
                "ServerId": master_server_id,
                "Port": port,
                "Weight": "100",
                "ServerType": "Master",
                "Type": "ecs_fnat"
            },
            {
                "ServerId": slave_server_id,
                "Port": port,
                "Weight": "100",
                "ServerType": "Slave",
                "Type": "ecs_fnat"
            }
        ]

        request.set_LoadBalancerId(load_balance_id)
        request.set_MasterSlaveServerGroupName(group_name)

        request.set_MasterSlaveBackendServers(json.dumps(servers))
        response = self.__do_action(request)
        return response

    def create_vserver_group(self, load_balance_id, group_name, servers):
        request = CreateVServerGroupRequest()
        request.set_accept_format('json')
        request.set_LoadBalancerId(load_balance_id)
        request.set_VServerGroupName(group_name)
        request.set_BackendServers(servers)
        response = self.__do_action(request)
        return response

    def create_load_balance_tcp_listener(self, load_balance_id, server_ip, port):
        request = CreateLoadBalancerTCPListenerRequest()
        request.set_accept_format('json')

        request.set_ListenerPort(port)
        request.set_BackendServerPort(port)
        request.set_Bandwidth(-1)
        request.set_LoadBalancerId(load_balance_id)

        request.set_MasterSlaveServerGroupId(server_group_id)
        response = self.__do_action(request)
        return response

    def create_load_balance_tcp_listener_by_master_slave_group(self, load_balance_id, server_group_id, port):
        request = CreateLoadBalancerTCPListenerRequest()
        request.set_accept_format('json')

        request.set_ListenerPort(port)
        request.set_Bandwidth(-1)
        request.set_LoadBalancerId(load_balance_id)
        # request.set_Scheduler("tch")
        request.set_MasterSlaveServerGroupId(server_group_id)
        response = self.__do_action(request)
        return response

    def create_load_balance_tcp_listener_by_vserver_group(self, load_balance_id, server_group_id, port):
        request = CreateLoadBalancerTCPListenerRequest()
        request.set_accept_format('json')

        request.set_ListenerPort(port)
        request.set_Bandwidth(-1)
        request.set_LoadBalancerId(load_balance_id)
        request.set_VServerGroupId(server_group_id)
        response = self.__do_action(request)
        return response

    def start_load_balance_listener(self, load_balance_id, port):
        request = StartLoadBalancerListenerRequest()
        request.set_accept_format('json')

        request.set_ListenerPort(port)
        request.set_LoadBalancerId(load_balance_id)

        response = self.__do_action(request)
        return response

    def delete_load_balance(self, load_balance_id):
        request = DeleteLoadBalancerRequest()
        request.set_accept_format('json')
        request.set_LoadBalancerId(load_balance_id)
        response = self.__do_action(request)
        return response

    def delete_master_slave_group(self, master_slave_group_id):
        request = DeleteMasterSlaveServerGroupRequest()
        request.set_accept_format('json')

        request.set_MasterSlaveServerGroupId(master_slave_group_id)
        response = self.__do_action(request)
        return response

    def delete_loadbalance_listener(self, load_balance_id, port):
        request = DeleteLoadBalancerListenerRequest()
        request.set_accept_format('json')

        request.set_ListenerPort(port)
        request.set_LoadBalancerId(load_balance_id)
        response = self.__do_action(request)
        return response

    def create_load_balance(self, network_type, load_balance_name, master_zone_id, slave_zone_id, load_balance_spec):
        request = CreateLoadBalancerRequest()
        request.set_accept_format('json')

        request.set_AddressType(network_type)
        request.set_LoadBalancerName(load_balance_name)
        request.set_MasterZoneId(master_zone_id)
        request.set_SlaveZoneId(slave_zone_id)
        request.set_LoadBalancerSpec(load_balance_spec)

        return self.__do_action(request)

    def describe_load_balance_attribute(self, load_balance_id):
        request = DescribeLoadBalancerAttributeRequest()
        request.set_accept_format('json')

        request.set_LoadBalancerId(load_balance_id)

        response = self.__do_action(request)
        return response

    def stop_load_balance_listener(self, load_balance_id, port):
        request = StopLoadBalancerListenerRequest()
        request.set_accept_format('json')

        request.set_ListenerPort(port)
        request.set_LoadBalancerId(load_balance_id)
        response = self.__do_action(request)
        return response


# instance_id = "i-zm0ge2f18238nnsxbn55"
# instance_type = "EcsInstance"
# region_no = "cn-hangzhou"
# response = Slb(region_no, accessKeyId, securityToken).create_single_tunnel_load_balance(instance_id, instance_type)
# print(response)

### non-fin
region_no = "cn-hangzhou"
# nonfin
# accessKeyId = ""
# securityToken = ""
# fin
accessKeyId = ""
securityToken = ""
uid = "1212121042863399"
vpc_id = "vpc-bp12v3efx5klzh720t5zf"
vsw_id = "vsw-bp1gd203fqmblp1iienks"
master_zone_id = "cn-hangzhou-b"
slave_zone_id = "cn-hangzhou-g"

load_balance_spec = "slb.s1.small"
load_balance_name = "oms-logproxy-slb-prod-hz"
resource_id = "vpc-bp1uelcb046nm660o8znm"

################## 1 ##################
# response = Slb(region_no, accessKeyId, securityToken).create_single_tunnel_load_balance(load_balance_spec,
#                                                                                         master_zone_id, slave_zone_id,
#                                                                                         vpc_id, vsw_id,
#                                                                                         load_balance_name, uid,
#                                                                                         resource_id)
# print(response)

################## 1.8 ##################
# Slb(region_no, accessKeyId, securityToken).modify_bypass_toa_attribute("i-zm06x63w1hbdwm7l2jns", 'ecs')
# Slb(region_no, accessKeyId, securityToken).modify_bypass_toa_attribute("i-zm0gmd8exql9gkp47zx4", 'ecs')

################## 2 ##################
# load_balance_id = "lb-t4nn0q3etzyl7la9asl6f" // singapore logproxy SLB
load_balance_id = "lb-pz5qcf7nch6ikfivg5anx"
vserver_group_name = "oms-logproxy-group"
port = 2983
servers = [
    {
        "Type": "ecs_fnat",
        "ServerId": "i-zm06x63w1hbdwm7l2jns",
        "ServerIp": "192.168.34.196",
        "Port": 2983,
        "Weight": "100"
    }
]
# response = Slb(region_no, accessKeyId, securityToken).create_vserver_group(load_balance_id, vserver_group_name,
#                                                                            json.dumps(servers))
# print(response)

################## 3 ##################
server_group_id = "rsp-pz51tev9cpdm1"
response = Slb(region_no, accessKeyId, securityToken).create_load_balance_tcp_listener_by_vserver_group(load_balance_id,
                                                                                                        server_group_id,
                                                                                                        port)
print(response)

################## 4 ##################
response = Slb(region_no, accessKeyId, securityToken).start_load_balance_listener(load_balance_id, port)
print(response)
