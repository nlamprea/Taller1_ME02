#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ssid.h"
#include "ns3/yans-wifi-helper.h"

// Default Network Topology
//
//   Wifi 10.1.3.0
//                 AP                AP
//  *    *    *    *                 *    *    *    *
//  |    |    |    |    10.1.1.1
// n5   n6   n7   n0 -------------- n1   n2   n3   n4
//                   point-to-point  |    |    |    |
//                                   ================
//                                     wifi 10.1.2.0

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WIFI_NET");

//Funciones de callback para los traces de IPv4
static void Ipv4TxTrace(Ptr<const Packet> packet, Ptr<Ipv4> ipv4, uint32_t interface)
{
    Ipv4Header ipHeader;
    packet->PeekHeader(ipHeader);
    NS_LOG_INFO("Enviando paquete desde " << ipHeader.GetSource() << " hacia " << ipHeader.GetDestination());
}

static void Ipv4RxTrace(Ptr<const Packet> packet, Ptr<Ipv4> ipv4, uint32_t interface)
{
    Ipv4Header ipHeader;
    packet->PeekHeader(ipHeader);
    Ipv4InterfaceAddress localAddress = ipv4->GetAddress(interface, 0);
    NS_LOG_INFO("Recibiendo paquete en " << localAddress.GetLocal() << " desde " << ipHeader.GetSource() << " hacia " << ipHeader.GetDestination());
}


int main(int argc, char* argv[]){

    bool verbose = true;
    int nodesWifi1 = 5; // Number of nodes
    int nodesWifi2 = 5; // Number of nodes
    double totalTime = 100.0; // Total simulation time
    std::string rate("2048bps");
    std::string phyMode("DsssRate11Mbps");
    std::string tr_name("manet-routing-compare");
    // int nodeSpeed = 20; // in m/s
    // int nodePause = 0;  // in s


    CommandLine cmd(__FILE__);
    cmd.AddValue("nodesWifi1", "Numero de nodos de la red 1", nodesWifi1);
    cmd.AddValue("nodesWifi2", "Numero de nodos de la red 2", nodesWifi2);
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
    cmd.AddValue("totalTime", "Enable pcap tracing", totalTime);

    cmd.Parse(argc, argv);

    if (verbose)
    {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

    

    //create clusters
    NodeContainer wifiApNodes;
    wifiApNodes.Create(2);

    //conexion punto a punto
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer p2pDevices;
    p2pDevices = pointToPoint.Install(wifiApNodes);


    //create nodes
    NodeContainer wifiStaNodes1;
    wifiStaNodes1.Create(nodesWifi1);
    NodeContainer wifiApNode1 = wifiApNodes.Get(1);
    NodeContainer wifiStaNodes2;
    wifiStaNodes2.Create(nodesWifi2);
    NodeContainer wifiApNode2 = wifiApNodes.Get(0);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");

    WifiHelper wifi;


    NodeContainer wifiApNode;
    wifiApNode.Add(wifiApNodes.Get(0)); // AP1
    wifiApNode.Create(1); // AP2    

    //NodeContainer wifiApNode2 = wifiApNodes.Get(1);


    //contenedor de dispositivos
    NetDeviceContainer staDevice1;
    NetDeviceContainer staDevice2;
    NetDeviceContainer wifiApNodes1;
    NetDeviceContainer wifiApNodes2;

    // Network 1
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    staDevice1 = wifi.Install(phy, mac, wifiStaNodes1);

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    wifiApNodes1 = wifi.Install(phy, mac, wifiApNode1);

    // Network 2
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    staDevice2 = wifi.Install(phy, mac, wifiStaNodes1);

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    wifiApNodes2 = wifi.Install(phy, mac, wifiApNode2);

    // movilidad de los nodos   
    MobilityHelper mobility;

    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX",
                                  DoubleValue(0.0),
                                  "MinY",
                                  DoubleValue(0.0),
                                  "DeltaX",
                                  DoubleValue(5.0),
                                  "DeltaY",
                                  DoubleValue(10.0),
                                  "GridWidth",
                                  UintegerValue(3),
                                  "LayoutType",
                                  StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel","Bounds",RectangleValue(Rectangle(-100, 100, -100, 100)));
    mobility.Install(wifiStaNodes1);
    mobility.Install(wifiStaNodes2);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiStaNodes1);
    stack.Install(wifiStaNodes2);
    stack.Install(wifiApNodes);

    //asignacion de direcciones IP (host y mascara)
    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces;
    p2pInterfaces = address.Assign(p2pDevices);

    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer StaInterface1;
    StaInterface1 = address.Assign(staDevice1);
    Ipv4InterfaceContainer ApInterface1;
    ApInterface1 = address.Assign(wifiApNodes1);

    address.SetBase("192.168.2.0", "255.255.255.0");
    Ipv4InterfaceContainer StaInterface2;
    StaInterface2 = address.Assign(staDevice2);
    Ipv4InterfaceContainer ApInterface2;
    ApInterface2 = address.Assign(wifiApNodes2);

    //habilitar trazas de los paquetes

    // UdpEchoServerHelper echoServer(9);

    // ApplicationContainer serverApps = echoServer.Install(wifiStaNodes1.Get(nodesWifi1 - 1));
    // serverApps.Start(Seconds(1.0));
    // serverApps.Stop(Seconds(10.0));

    // UdpEchoClientHelper echoClient(StaInterface1.GetAddress(nodesWifi1), 9);
    // echoClient.SetAttribute("MaxPackets", UintegerValue(1));
    // echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    // echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    // ApplicationContainer clientApps = echoClient.Install(wifiStaNodes2.Get(nodesWifi2 - 1));
    // clientApps.Start(Seconds(2.0));
    // clientApps.Stop(Seconds(10.0));

    // Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Simulator::Stop(Seconds(10.0));

    // Conectar los traces a los nodos cliente y servidor
    Ptr<Node> clientNode = wifiStaNodes1.Get(nodesWifi1 - 1);
    Ptr<Node> serverNode = wifiStaNodes2.Get(nodesWifi1 - 1);

    Ptr<Ipv4> clientIpv4 = clientNode->GetObject<Ipv4>();
    clientIpv4->TraceConnectWithoutContext("Tx", MakeCallback(&Ipv4TxTrace));
    clientIpv4->TraceConnectWithoutContext("Rx", MakeCallback(&Ipv4RxTrace));

    Ptr<Ipv4> serverIpv4 = serverNode->GetObject<Ipv4>();
    serverIpv4->TraceConnectWithoutContext("Tx", MakeCallback(&Ipv4TxTrace));
    serverIpv4->TraceConnectWithoutContext("Rx", MakeCallback(&Ipv4RxTrace));

    Simulator::Stop(Seconds(10.0));


    Simulator::Run();
    Simulator::Destroy();
    return 0;
}