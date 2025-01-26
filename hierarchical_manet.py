import ns.applications as apps
import ns.core as core
import ns.internet as internet
import ns.mobility as mobility
import ns.network as network
import ns.wifi as wifi
import ns.flow_monitor as flowmon

# Habilitar el registro para depuración
core.LogComponentEnable("HierarchicalMANET", core.LOG_LEVEL_INFO)

# Crear nodos para los clústeres
cluster1 = network.NodeContainer()
cluster2 = network.NodeContainer()
bridge_cluster = network.NodeContainer()

# Crear nodos en los clústeres
cluster1.Create(5)  # Clúster 1 con 5 nodos
cluster2.Create(5)  # Clúster 2 con 5 nodos
bridge_cluster.Create(1)  # Nodo puente para conectar clústeres

# Crear nodos líderes (cluster heads)
cluster1_head = network.Node()
cluster2_head = network.Node()

# Crear contenedor para puente entre clústeres
bridge_nodes = network.NodeContainer()
bridge_nodes.Add(cluster1_head)
bridge_nodes.Add(cluster2_head)
bridge_nodes.Add(bridge_cluster.Get(0))

# Función para configurar un clúster
def configure_cluster(cluster_head, cluster_nodes, range):
    """
    Configura un clúster en la red MANET.
    :param cluster_head: Nodo principal (líder del clúster)
    :param cluster_nodes: Nodos del clúster
    :param range: Rango de movilidad y posiciones
    """
    # Configurar dispositivos Wi-Fi
    wifi_helper = wifi.WifiHelper()
    wifi_helper.SetStandard(wifi.WIFI_PHY_STANDARD_80211g)
    
    mac = wifi.WifiMacHelper()
    mac.SetType("ns3::AdhocWifiMac")  # Configuración para modo ad hoc
    
    phy = wifi.YansWifiPhyHelper.Default()
    channel = wifi.YansWifiChannelHelper.Default()
    phy.SetChannel(channel.Create())
    
    # Instalar dispositivos Wi-Fi en los nodos
    devices = wifi_helper.Install(phy, mac, cluster_nodes)
    devices.Add(wifi_helper.Install(phy, mac, cluster_head))
    
    # Configurar pila de protocolos de Internet
    internet_stack = internet.InternetStackHelper()
    internet_stack.Install(cluster_nodes)
    internet_stack.Install(cluster_head)
    
    # Asignar direcciones IP
    ipv4 = internet.Ipv4AddressHelper()
    ipv4.SetBase("10.{}.0.0".format(range), "255.255.255.0")
    ipv4.Assign(devices)
    
    # Configurar movilidad de los nodos
    mobility_helper = mobility.MobilityHelper()
    position_alloc = mobility.ListPositionAllocator()
    
    # Asignar posiciones para los nodos
    for i in range(cluster_nodes.GetN()):
        position_alloc.Add(core.Vector3D(range + i * 10.0, range, 0.0))
    position_alloc.Add(core.Vector3D(range, range, 0.0))  # Nodo principal
    
    mobility_helper.SetPositionAllocator(position_alloc)
    mobility_helper.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                     "Bounds", core.RectangleValue(core.Rectangle(-range, range, -range, range)))
    mobility_helper.Install(cluster_nodes)
    mobility_helper.Install(cluster_head)

# Configurar los tres clústeres
configure_cluster(cluster1_head, cluster1, 50)  # Clúster 1
configure_cluster(cluster2_head, cluster2, 150)  # Clúster 2
configure_cluster(bridge_cluster.Get(0), bridge_nodes, 100)  # Nodo puente

# Configurar tráfico CBR (Constant Bit Rate)
on_off_helper = apps.OnOffHelper("ns3::UdpSocketFactory", 
                                 internet.Ipv4AddressHelper.GetAddress("10.50.0.1", 9))
on_off_helper.SetConstantRate(core.DataRate("500kbps"))  # Tasa constante de datos
on_off_apps = on_off_helper.Install(cluster1.Get(0))  # Instalar en el primer nodo del clúster 1
on_off_apps.Start(core.Seconds(1.0))  # Inicio del tráfico
on_off_apps.Stop(core.Seconds(10.0))  # Fin del tráfico

# Configurar tráfico FTP
ftp_helper = apps.BulkSendHelper("ns3::TcpSocketFactory", 
                                 internet.Ipv4AddressHelper.GetAddress("10.150.0.1", 20))
ftp_helper.SetAttribute("MaxBytes", core.UintegerValue(1024 * 1024))  # Limitar a 1 MB
ftp_apps = ftp_helper.Install(cluster2.Get(0))  # Instalar en el primer nodo del clúster 2
ftp_apps.Start(core.Seconds(1.0))  # Inicio del tráfico
ftp_apps.Stop(core.Seconds(10.0))  # Fin del tráfico

# Configurar FlowMonitor para analizar resultados
flow_monitor_helper = flowmon.FlowMonitorHelper()
monitor = flow_monitor_helper.InstallAll()

# Ejecutar simulación
core.Simulator.Stop(core.Seconds(10.0))
core.Simulator.Run()

# Analizar resultados con FlowMonitor
monitor.SerializeToXmlFile("manet_hierarchical_simulation.xml", True, True)

core.Simulator.Destroy()
