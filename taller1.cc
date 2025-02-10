#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/aodv-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/dsr-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/olsr-module.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Taller1ME02");

std::string m_CSVfileName{"/home/nicolas/ns-allinone-3.43/ns-3.43/scratch/manet-routing.output.csv"}; //!< CSV filename.
uint128_t bytesTotal = 0;      //!< Total received bytes.
uint32_t packetsReceived = 0 ;
int m_nSinks{10};   
std::string m_protocolName{"AODV"};    
double m_txp{7.5};  
bool m_traceMobility{true};                           //!< Enable mobility tracing.
bool m_flowMonitor{false};   
// Funciones de callback para los traces de IPv4
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


static std::string PrintReceivedPacket(Ptr<Socket> socket, Ptr<Packet> packet, Address senderAddress)
{
    std::ostringstream oss;

    oss << Simulator::Now().GetSeconds() << " " << socket->GetNode()->GetId();

    if (InetSocketAddress::IsMatchingType(senderAddress))
    {
        InetSocketAddress addr = InetSocketAddress::ConvertFrom(senderAddress);
        oss << " received one packet from " << addr.GetIpv4();
    }
    else
    {
        oss << " received one packet!";
    }
    return oss.str();
}

static void ReceivePacket(Ptr<Socket> socket)
{
    Ptr<Packet> packet;
    Address senderAddress;
    while ((packet = socket->RecvFrom(senderAddress)))
    {
        bytesTotal += packet->GetSize();
        packetsReceived += 1;
        NS_LOG_UNCOND(PrintReceivedPacket(socket, packet, senderAddress));
    }
}

static void CheckThroughput()
{
    double kbs = (bytesTotal * 8.0) / 1000;
    bytesTotal = 0;

    std::ofstream out(m_CSVfileName, std::ios::app);

    out << (Simulator::Now()).GetSeconds() << "," << kbs << "," << packetsReceived << ","
        << m_nSinks << "," << m_protocolName << "," << m_txp << "" << std::endl;

    out.close();
    packetsReceived = 0;
    Simulator::Schedule(Seconds(1.0), &CheckThroughput);
}

Ptr<Socket> SetupPacketReceive(Ipv4Address addr, Ptr<Node> node)
{
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    Ptr<Socket> sink = Socket::CreateSocket(node, tid);
    InetSocketAddress local = InetSocketAddress(addr, port);
    sink->Bind(local);
    sink->SetRecvCallback(MakeCallback(ReceivePacket));

    return sink;
}


int main(int argc, char *argv[]) {

    Packet::EnablePrinting();
    // blank out the last output file and write the column headers
    std::ofstream out(m_CSVfileName);
    out << "SimulationSecond,"
        << "ReceiveRate,"
        << "PacketsReceived,"
        << "NumberOfSinks,"
        << "RoutingProtocol,"
        << "TransmissionPower" << std::endl;
    out.close();

    bool packetReceive = true;
    bool verbose = false;
    bool printRoutes = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("packetReceive", "Ver paquetes", packetReceive);
    cmd.AddValue("protocol", "Routing protocol (OLSR, AODV, DSDV)", m_protocolName);
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);


    cmd.Parse(argc, argv);
    // Enable logging
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

    // Create nodes for WiFi stations and APs
    NodeContainer wifiStaNodes1, wifiStaNodes2;
    wifiStaNodes1.Create(2); // Two stations in first WiFi
    wifiStaNodes2.Create(2); // Two stations in second WiFi

    NodeContainer wifiApNodes;
    wifiApNodes.Create(2); // Two AP nodes

    // Set up P2P link between APs
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer p2pDevices;
    p2pDevices = p2p.Install(wifiApNodes.Get(0), wifiApNodes.Get(1));

    // Configure WiFi networks
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    YansWifiChannelHelper channel;
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss("ns3::FriisPropagationLossModel");
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    // First WiFi network
    Ssid ssid1 = Ssid("Network1");
    WifiMacHelper mac;

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid1),
                "ActiveProbing", BooleanValue(true));
    NetDeviceContainer staDevices1 = wifi.Install(phy, mac, wifiStaNodes1);

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid1));
    NetDeviceContainer apDevices1 = wifi.Install(phy, mac, wifiApNodes.Get(0));

    // Second WiFi network
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

    // Install Internet stack with AODV routing

    AodvHelper aodv;
    OlsrHelper olsr;
    DsdvHelper dsdv;
    DsrHelper dsr;
    Ipv4ListRoutingHelper list;
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
    // AodvHelper aodv;
    // InternetStackHelper stack;
    // stack.SetRoutingHelper(aodv);
    // stack.Install(wifiStaNodes1);
    // stack.Install(wifiStaNodes2);
    // stack.Install(wifiApNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;

    // First WiFi network
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces1 = address.Assign(staDevices1);
    address.Assign(apDevices1);

    // Second WiFi network
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces2 = address.Assign(staDevices2);
    address.Assign(apDevices2);

    // P2P network
    address.SetBase("10.1.3.0", "255.255.255.252");
    Ipv4InterfaceContainer p2pInterfaces = address.Assign(p2pDevices);


    OnOffHelper onoff1("ns3::UdpSocketFactory", Address());
    onoff1.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    onoff1.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));

    for (int i = 0; i < m_nSinks; i++)
    {
        Ptr<Socket> sink = SetupPacketReceive(staInterfaces1.GetAddress(i), adhocNodes.Get(i));

        AddressValue remoteAddress(InetSocketAddress(adhocInterfaces.GetAddress(i), port));
        onoff1.SetAttribute("Remote", remoteAddress);

        Ptr<UniformRandomVariable> var = CreateObject<UniformRandomVariable>();
        ApplicationContainer temp = onoff1.Install(adhocNodes.Get(i + m_nSinks));
        temp.Start(Seconds(var->GetValue(100.0, 101.0)));
        temp.Stop(Seconds(TotalTime));
    }

    // Applications (UDP Echo for testing)
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(wifiStaNodes2.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(staInterfaces2.GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(2));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(wifiStaNodes1.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable PCAP tracing
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


    if (printRoutes)
     {
         Ptr<OutputStreamWrapper> routingStream =
             Create<OutputStreamWrapper>("/home/nicolas/ns-allinone-3.43/ns-3.43/scratch/taller.routes", std::ios::out);
         Ipv4RoutingHelper::PrintRoutingTableAllAt(Seconds(8), routingStream);
     }

    CheckThroughput();
    // Run simulation
    Simulator::Stop(Seconds(100.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}