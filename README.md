Kernel Space TAP Tunnel Example
===============================

This is an example of TAP tunnel in kernel space.

Only the following packets can be passed in the TAP tunnel:

* ARP request / reply
* IPV4 ICMP request / reply
* IPV4 TCP packets
* IPV4 UDP packets

The Tunnel is built in UDP with source port 50000 = destination port



File Descriptions
-----------------

| File        | Descriptions                                             |
|-------------|----------------------------------------------------------|
| kmain.c     | kernel init / exit functions.                            |
| ksyscall.c  | re-write some system calls in kernel space.              |
| ktap.c      | creating and using tap tunnel in kernel space.           |
| kudp.c      | creating and using udp socket in kernel space.           |
| ktunnel.h   | linux heraders, defined values, macros, type / function declarations. |
|-------------|----------------------------------------------------------|
| knetpoll.c  | using netpoll APIs to send udp packets.                  |
|-------------|----------------------------------------------------------|
| kfilter.c   | netfilter hook function for receiving tunnel data.       |
| ktuunel_wireshark.lua | simple wireshark dissector for this project.   |



Verification Environment
------------------------

This project works in the following linux distributions:

* Ubuntu 14.04 i386 and amd64 - Kernel version 3.13.0
* Mint 17 i386 and amd64      - Kernel version 3.13.0



How to test?
------------

Prepare 2 computers : COMPUTER A and COMPUTER B

* COMPUTER A : Real ip address 192.168.1.1
* COMPUTER B : Real ip address 192.168.1.2

Build your proejct, and insert kernel module in COMPUTER A:

    $ cd kernel_tap/

    $ make

    $ sudo insmod ktunnel.ko g_dstRealip="192.168.1.2" g_ip="10.10.10.1" \
      g_mask="255.255.255.0" g_tunnelPort=50000

    $ ifconfig

You will see the new network interface "tap01" with ipaddr "10.10.10.1"

Do it again in COMPUTER B:

    $ sudo insmod ktunnel.ko g_dstRealip="192.168.1.1" g_ip="10.10.10.2" \
      g_mask="255.255.255.0" g_tunnelPort=50000

you will see the new network interface "tap01" with ipaddr "10.10.10.2"

In COMPUTER A, do "$ ping 10.10.10.2", and you will see ping success.

If you check packets in wireshark, you will see the ARP and ICMP packets encapsulated in the UDP Tunnel.




tag: v1.0 (tx: udp, rx: udp)
----------------------------

             COMPUTER A                           COMPUTER B
        192.186.1.1(10.10.10.1)              192.168.1.2(10.10.10.2)

              process                              process
                 |                                    |
                 |                USER                |
        --------------------                --------------------
                 |               KERNEL               |
             +--------+                          +--------+
             |  KTAP  |                          |  KTAP  |
             +--------+                          +--------+
                 | read / write                       |  read / write
                 |                                    |
             +--------+                          +--------+
             |  KUDP  |--------------------------|  KUDP  |
             +--------+       send / recv        +--------+




tag: v2.0  (tx: udp or netpoll, rx: udp)
----------------------------------------

If you insert module with different module parameter 'txmode', there will be some different with the above structure.

    $ sudo insmod ktunnel.ko g_dstRealip="192.168.1.1" g_ip="10.10.10.2" \
      g_mask="255.255.255.0" g_tunnelPort=50000 g_txmode="netpoll"

Netpoll tx mode will decrease a little CPU usage in sender COMPUTER.

                 COMPUTER A                           COMPUTER B
            192.186.1.1(10.10.10.1)              192.168.1.2(10.10.10.2)

                  process                              process
                     |                                    |
                     |                USER                |
            --------------------                --------------------
                     |               KERNEL               |
                 +--------+                          +--------+
          +----->|  KTAP  |                          |  KTAP  |--->--+
          |      +--------+                          +--------+      |
          |          | read                               ^  write   |
          ^          |                                    |          v
          |          v                                    |          |
    write |     +----------+                         +--------+      | read
          ^     | KNETPOLL |---->------>------>------|  KUDP  |      v
          |     +----------+  send              recv +--------+      |
          |                                                          |
          |      +--------+                         +----------+     |
          +--<---|  KUDP  |-----<------<------<-----| KNETPOLL |<----+
                 +--------+ recv               send +----------+





tag: v3.0 (tx: udp or netpoll, rx: udp or netfilter)
----------------------------------------------------

If you insert module with different module parameter 'rxmode', there will be some different with the above structures.

    $ sudo insmod ktunnel.ko g_dstRealip="192.168.1.1" g_ip="10.10.10.2" \
      g_mask="255.255.255.0" g_tunnelPort=50000 g_rxmode="filter"

Netfilter rx mode will decrease a little CPU usage in receiver COMPUTER.

                 COMPUTER A                           COMPUTER B
            192.186.1.1(10.10.10.1)              192.168.1.2(10.10.10.2)

                  process                              process
                     |                                    |
                     |                USER                |
            --------------------                --------------------
                     |               KERNEL               |
                 +--------+                          +--------+
          +----->|  KTAP  |                          |  KTAP  |--->--+
          |      +--------+                          +--------+      |
          |          | read                               ^  write   |
          ^          |                                    |          v
          |          v                                    |          |
    write |      +--------+                          +---------+     | read
          ^      |  KUDP  |----->------>------>------| KFILTER |     v
          |      +--------+ send           netfilter +---------+     |
          |                                                          |
          |     +---------+                          +--------+      |
          +--<--| KFILTER |-----<------<------<------|  KUDP  |<-----+
                +---------+ netfilter           send +--------+

    $ sudo insmod ktunnel.ko g_dstRealip="192.168.1.1" g_ip="10.10.10.2" \
      g_mask="255.255.255.0" g_tunnelPort=50000 \
      g_txmode="netpoll" g_rxmode="filter"


                 COMPUTER A                           COMPUTER B
            192.186.1.1(10.10.10.1)              192.168.1.2(10.10.10.2)

                  process                              process
                     |                                    |
                     |                USER                |
            --------------------                --------------------
                     |               KERNEL               |
                 +--------+                          +--------+
          +----->|  KTAP  |                          |  KTAP  |--->--+
          |      +--------+                          +--------+      |
          |          | read                               ^  write   |
          ^          |                                    |          v
          |          v                                    |          |
    write |     +----------+                         +---------+     | read
          ^     | KNETPOLL |----->----->------>------| KFILTER |     v
          |     +----------+ send          netfilter +---------+     |
          |                                                          |
          |     +---------+                         +----------+     |
          +--<--| KFILTER |-----<------<------<-----| KNETPOLL |<----+
                +---------+ netfilter          send +----------+





How to use wireshark dissector?
-------------------------------

Check wireshark is installed in your computer and works.

Find the installed path of wireshark, In windows default path is C:\Program Files\Wireshark\

Put "ktuunel_wireshark.lua" to the wireshark installed path.

Open init.lua (in the isntalled path) in your text editor

Go to the file bottom. find the line:

        dofile(DATA_DIR.."console.lua")

Add a new line after it:

        dofile(DATA_DIR.."ktunnel_wireshark.lua")

Save the init.lua, and re-open (not refresh) you wireshark.

The simple dissector will works.

You could use "ktunnel" as the keyword to filter packets.




References
----------

1.[Example of TUN in User Space](http://neokentblog.blogspot.tw/2014/05/linux-virtual-interface-tuntap.html)

2.[Example of Kernel Space UDP Socket](http://kernelnewbies.org/Simple_UDP_Server)

3.[Example of Sending UDP by Netpoll APIs](http://goo.gl/is95GX)

4.[Example of Net Filter Hook](http://neokentblog.blogspot.tw/2014/06/netfilter-hook.html)

