// Incluir módulos necesarios de ns-3
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-module.h"
// Protocolos de enrutamiento
#include "ns3/aodv-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/dsr-module.h"
#include "ns3/olsr-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/yans-wifi-helper.h"
#include <fstream>

// Default Network Topology
//
//   Wifi 10.1.3.0
//                 AP
//  *    *    *    *
//  |    |    |    |    10.1.1.1
// n5   n6   n7   n0 -------------- n1   n2   n3   n4
//                   point-to-point  |    |    |    |
//                                   ================
//                                     wifi 10.1.2.0


using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Taller1ME02");

static std::ofstream csvFile;

// Función de trace para paquetes transmitidos
static void Ipv4TxTrace(Ptr<const Packet> packet, Ptr<Ipv4> ipv4, uint32_t interface) {
    Ipv4Header ipHeader;
    if (packet->PeekHeader(ipHeader)) {
        csvFile << ipHeader.GetSource() << ","
                << ipHeader.GetDestination() << ","
                << packet->GetSize() << ","
                << ipHeader.GetProtocol() << ","
                << "1" << std::endl;
        NS_LOG_INFO("Enviando paquete desde " << ipHeader.GetSource() << " hacia " << ipHeader.GetDestination());
    }
}

// Función de trace para paquetes recibidos
static void Ipv4RxTrace(Ptr<const Packet> packet, Ptr<Ipv4> ipv4, uint32_t interface)
{
    Ipv4Header ipHeader;
    packet->PeekHeader(ipHeader);
    Ipv4InterfaceAddress localAddress = ipv4->GetAddress(interface, 0);
    NS_LOG_INFO("Recibiendo paquete en " << localAddress.GetLocal() << " desde " << ipHeader.GetSource() << " hacia " << ipHeader.GetDestination());
}


int main(int argc, char *argv[]) {

    Packet::EnablePrinting();
    std::string m_protocolName{"AODV"}; // Protocolo de enrutamiento por defecto

    // Inicializar archivo CSV
    csvFile.open("/home/nicolas/ns-allinone-3.43/ns-3.43/scratch/packet_info.csv");
    csvFile << "SourceIP/Fuente,DestinationIP/Destino,PacketSize/TamañoPaquete,Protocol/Protocolo,NumPackets/NumPaquetes\n";

    // Parámetros configurables
    bool packetReceive = true;
    bool verbose = false;
    bool printRoutes = true;

    // Manejo de parámetros de línea de comandos
    CommandLine cmd(__FILE__);
    cmd.AddValue("packetReceive", "Ver paquetes", packetReceive);
    cmd.AddValue("protocol", "Routing protocol (OLSR, AODV, DSDV)", m_protocolName);
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);

    cmd.Parse(argc, argv);

    // Configurar logging
    if (verbose)
    {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
        LogComponentEnable("Aodv    RoutingProtocol", LOG_LEVEL_LOGIC);
    }
    
    if (packetReceive)
    {
        LogComponentEnable("Taller1ME02", LOG_LEVEL_INFO); // Habilita los logs del script
    }

    // ====================== CONFIGURACIÓN DE RED ======================
    // Creación de nodos
    NodeContainer wifiStaNodes1, wifiStaNodes2;
    wifiStaNodes1.Create(2); // Red 1: 2 estaciones
    wifiStaNodes2.Create(2); // Red 2: 2 estaciones

    NodeContainer wifiApNodes;
    wifiApNodes.Create(2); // 2 APs

    // Set up P2P link between APs
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer p2pDevices;
    p2pDevices = p2p.Install(wifiApNodes.Get(0), wifiApNodes.Get(1));

    NodeContainer allNodes;
    allNodes.Add(wifiStaNodes1);
    allNodes.Add(wifiStaNodes2);
    allNodes.Add(wifiApNodes);

    // ====================== CONFIGURACIÓN WiFi ======================

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    // Configuración del canal inalámbrico
    YansWifiChannelHelper channel;
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss("ns3::FriisPropagationLossModel");
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    // Red WiFi 1 (AP + estaciones)
    Ssid ssid1 = Ssid("Network1");
    WifiMacHelper mac;

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid1),
                "ActiveProbing", BooleanValue(true));
    NetDeviceContainer staDevices1 = wifi.Install(phy, mac, wifiStaNodes1);

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid1));
    NetDeviceContainer apDevices1 = wifi.Install(phy, mac, wifiApNodes.Get(0));

    // Red WiFi 2 (AP + estaciones)
    Ssid ssid2 = Ssid("Network2");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid2),
                "ActiveProbing", BooleanValue(true));
    NetDeviceContainer staDevices2 = wifi.Install(phy, mac, wifiStaNodes2);

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid2));
    NetDeviceContainer apDevices2 = wifi.Install(phy, mac, wifiApNodes.Get(1));

    // Mobility configuration
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes1);
    mobility.Install(wifiStaNodes2);
    mobility.Install(wifiApNodes);

    // Position APs and stations
    // AP1 at (0,0), stations nearby
    Ptr<ConstantPositionMobilityModel> ap1Pos = wifiApNodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
    ap1Pos->SetPosition(Vector(0.0, 0.0, 0.0));

    for (uint32_t i = 0; i < wifiStaNodes1.GetN(); ++i) {
        Ptr<ConstantPositionMobilityModel> staPos = wifiStaNodes1.Get(i)->GetObject<ConstantPositionMobilityModel>();
        staPos->SetPosition(Vector(5.0, 0.0, 0.0));
    }

    // AP2 at (100,0), stations nearby
    Ptr<ConstantPositionMobilityModel> ap2Pos = wifiApNodes.Get(1)->GetObject<ConstantPositionMobilityModel>();
    ap2Pos->SetPosition(Vector(100.0, 0.0, 0.0));

    for (uint32_t i = 0; i < wifiStaNodes2.GetN(); ++i) {
        Ptr<ConstantPositionMobilityModel> staPos = wifiStaNodes2.Get(i)->GetObject<ConstantPositionMobilityModel>();
        staPos->SetPosition(Vector(105.0, 0.0, 0.0));
    }

    // ====================== PILA DE PROTOCOLOS ======================

    AodvHelper aodv;
    OlsrHelper olsr;
    DsdvHelper dsdv;
    DsrHelper dsr;
    Ipv4ListRoutingHelper list; // Helper para selección de protocolo de enrutamiento
    InternetStackHelper stack;
    if (m_protocolName == "OLSR")
    {
        list.Add(olsr, 100);
        stack.SetRoutingHelper(list);
        stack.Install(wifiStaNodes1);
        stack.Install(wifiStaNodes2);
        stack.Install(wifiApNodes);
    }
    else if (m_protocolName == "AODV")
    {
        list.Add(aodv, 100);
        stack.SetRoutingHelper(list);
        stack.Install(wifiStaNodes1);
        stack.Install(wifiStaNodes2);
        stack.Install(wifiApNodes);
    }
    else if (m_protocolName == "DSDV")
    {
        list.Add(dsdv, 100);
        stack.SetRoutingHelper(list);
        stack.Install(wifiStaNodes1);
        stack.Install(wifiStaNodes2);
        stack.Install(wifiApNodes);
    }
    else
    {
        NS_FATAL_ERROR("No such protocol:" << m_protocolName);
    }

    // ====================== ASIGNACIÓN DE DIRECCIONES IP ======================
    Ipv4AddressHelper address;

    // Red WiFi 1
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces1 = address.Assign(staDevices1);
    address.Assign(apDevices1);

    // Red WiFi 2
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces2 = address.Assign(staDevices2);
    address.Assign(apDevices2);

    // Enlace P2P
    address.SetBase("10.1.3.0", "255.255.255.252");
    Ipv4InterfaceContainer p2pInterfaces = address.Assign(p2pDevices);


    for (uint32_t i = 0; i < allNodes.GetN(); ++i) {
        Ptr<Node> node = allNodes.Get(i);
        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
        if (ipv4) {
            ipv4->TraceConnectWithoutContext("Tx", MakeCallback(&Ipv4TxTrace));
        }
    }

    // ====================== APLICACIONES ======================
    // Servidor UDP en red 2
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(wifiStaNodes2.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Cliente UDP en red 1
    UdpEchoClientHelper echoClient(staInterfaces2.GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(2));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(wifiStaNodes1.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Habilitar capturas PCAP
    phy.EnablePcapAll("two_wifi_aodv");


    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Conectar los traces a los nodos cliente y servidor
    Ptr<Node> clientNode = wifiStaNodes2.Get(0);
    Ptr<Node> serverNode = wifiStaNodes1.Get(0);

    Ptr<Ipv4> clientIpv4 = clientNode->GetObject<Ipv4>();
    clientIpv4->TraceConnectWithoutContext("Tx", MakeCallback(&Ipv4TxTrace));
    clientIpv4->TraceConnectWithoutContext("Rx", MakeCallback(&Ipv4RxTrace));

    Ptr<Ipv4> serverIpv4 = serverNode->GetObject<Ipv4>();
    serverIpv4->TraceConnectWithoutContext("Tx", MakeCallback(&Ipv4TxTrace));
    serverIpv4->TraceConnectWithoutContext("Rx", MakeCallback(&Ipv4RxTrace));

    // Generar tablas de enrutamiento
    if (printRoutes)
     {
         Ptr<OutputStreamWrapper> routingStream =
             Create<OutputStreamWrapper>("/home/nicolas/ns-allinone-3.43/ns-3.43/scratch/taller.routes", std::ios::out);
         Ipv4RoutingHelper::PrintRoutingTableAllAt(Seconds(8), routingStream);
     }

    // ====================== EJECUCIÓN DE LA SIMULACIÓN ======================
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    csvFile.close();
    Simulator::Destroy();

    return 0;
}