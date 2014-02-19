tayga --mktun
ip link set nat64 up 
ip addr add 172.16.220.128 dev nat64 
ip addr add aaaa::1 dev nat64 
ip route add 172.16.255.0/24 dev nat64 
ip route add aaaa:0:2:eeee::/96 dev nat64 
gnome-terminal -t tayga -x tayga -d -c ./tayga.conf &
sysctl -w net.ipv4.ip_forward=1
sysctl -w net.ipv6.conf.all.forwarding=1
