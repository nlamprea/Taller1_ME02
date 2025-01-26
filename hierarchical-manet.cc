#include "ns3/core-module.h" // Módulo central de NS-3 para configuración básica
#include "ns3/network-module.h" // Permite la creación y manejo de nodos y dispositivos de red
#include "ns3/mobility-module.h" // Define modelos de movilidad para los nodos
#include "ns3/internet-module.h" // Proporciona la pila de protocolos TCP/IP
#include "ns3/wifi-module.h" // Permite la configuración de dispositivos y canales Wi-Fi
#include "ns3/applications-module.h" // Contiene aplicaciones de tráfico como OnOff y FTP
#include "ns3/flow-monitor-module.h" // Herramienta para monitorear y analizar el rendimiento de la red

using namespace ns3; // Espacio de nombres de NS-3

NS_LOG_COMPONENT_DEFINE("HierarchicalMANET"); // Define un componente de log para rastrear la simulación

// Función para configurar un clúster
void ConfigureCluster(Ptr<Node> clusterHead, NodeContainer clusterNodes, double range) {
    // Configurar los dispositivos Wi-Fi
    WifiHelper wifi; // Ayuda para configurar Wi-Fi
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g); // Configura el estándar Wi-Fi 802.11g

    WifiMacHelper mac; // Ayuda para configurar la capa MAC
    mac.SetType("ns3::AdhocWifiMac"); // Configura el modo Ad-Hoc

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default(); // Configuración física Wi-Fi por defecto
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default(); // Configura el canal inalámbrico por defecto
    phy.SetChannel(channel.Create()); // Asocia el canal con el dispositivo físico

    // Instalar dispositivos Wi-Fi en los nodos del clúster
    NetDeviceContainer devices = wifi.Install(phy, mac, clusterNodes);
    devices.Add(wifi.Install(phy, mac, clusterHead)); // Agregar el nodo líder del clúster

    // Instalar la pila de protocolos TCP/IP
    InternetStackHelper internet;
    internet.Install(clusterNodes); // Instala la pila en los nodos del clúster
    internet.Install(clusterHead); // Instala la pila en el líder del clúster

    // Asignar direcciones IP a los nodos
    Ipv4AddressHelper address;
    address.SetBase("10.1.0.0", "255.255.255.0"); // Rango de direcciones IP para el clúster
    address.Assign(devices); // Asigna las direcciones a los dispositivos Wi-Fi

    // Configurar la movilidad de los nodos
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    for (uint32_t i = 0; i < clusterNodes.GetN(); ++i) {
        positionAlloc->Add(Vector(range + i * 10.0, range, 0.0)); // Posiciona los nodos en una línea
    }
    positionAlloc->Add(Vector(range, range, 0.0)); // Posiciona el líder del clúster
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", // Configura un modelo de movimiento aleatorio
                              "Bounds", RectangleValue(Rectangle(-range, range, -range, range))); // Define límites de movimiento
    mobility.Install(clusterNodes); // Aplica la movilidad a los nodos del clúster
    mobility.Install(clusterHead); // Aplica la movilidad al líder del clúster
}

int main(int argc, char *argv[]) {
    LogComponentEnable("HierarchicalMANET", LOG_LEVEL_INFO); // Habilita los logs para rastrear la ejecución

    // Crear nodos para los clústeres
    NodeContainer cluster1, cluster2, bridgeCluster;
    cluster1.Create(5); // Clúster 1 con 5 nodos
    cluster2.Create(5); // Clúster 2 con 5 nodos
    bridgeCluster.Create(1); // Nodo puente para conectar los clústeres

    Ptr<Node> cluster1Head = CreateObject<Node>(); // Nodo líder del clúster 1
    Ptr<Node> cluster2Head = CreateObject<Node>(); // Nodo líder del clúster 2

    // Configurar clústeres
    ConfigureCluster(cluster1Head, cluster1, 50.0); // Configura el clúster 1
    ConfigureCluster(cluster2Head, cluster2, 150.0); // Configura el clúster 2

    // Configurar puente entre clústeres
    NodeContainer bridgeNodes;
    bridgeNodes.Add(cluster1Head); // Agregar líder del clúster 1
    bridgeNodes.Add(cluster2Head); // Agregar líder del clúster 2
    bridgeNodes.Add(bridgeCluster.Get(0)); // Agregar nodo puente
    ConfigureCluster(bridgeCluster.Get(0), bridgeNodes, 100.0); // Configura el puente

    // Configurar tráfico de tipo CBR
    OnOffHelper onOff("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address("10.1.0.255"), 9)); // CBR con tráfico UDP
    onOff.SetConstantRate(DataRate("500kbps")); // Configura la tasa constante de datos
    ApplicationContainer apps = onOff.Install(cluster1.Get(0)); // Instala la aplicación en el nodo 0 del clúster 1
    apps.Start(Seconds(1.0)); // Inicia el tráfico en el segundo 1
    apps.Stop(Seconds(10.0)); // Finaliza el tráfico en el segundo 10

    // Configurar tráfico FTP (TCP)
    BulkSendHelper bulkSend("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address("10.1.1.1"), 20)); // Tráfico FTP
    bulkSend.SetAttribute("MaxBytes", UintegerValue(1024 * 1024)); // Límite de 1 MB
    apps = bulkSend.Install(cluster2.Get(0)); // Instala la aplicación en el nodo 0 del clúster 2
    apps.Start(Seconds(1.0)); // Inicia el tráfico en el segundo 1
    apps.Stop(Seconds(10.0)); // Finaliza el tráfico en el segundo 10

    // Configurar FlowMonitor para análisis de rendimiento
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll(); // Instala FlowMonitor en todos los nodos

    Simulator::Stop(Seconds(10.0)); // Configura la duración de la simulación
    Simulator::Run(); // Ejecuta la simulación

    // Guardar resultados en un archivo XML
    monitor->SerializeToXmlFile("manet-simulation.xml", true, true); // Exporta métricas a un archivo

    Simulator::Destroy(); // Limpia los recursos después de la simulación
    return 0;
}
