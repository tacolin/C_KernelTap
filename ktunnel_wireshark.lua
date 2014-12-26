do
    local ktun_proto  = Proto("ktunnel", "KERNEL TAP TUNNEL")
    ktun_proto.fields = {}

    local fds = ktun_proto.fields

    ktun_proto.dissector = function (buffer, info, root_tree)

        local ktun_tree = root_tree:add(ktun_proto, buffer(0), "KTUNNEL DATA")
        local eth_dissector = Dissector.get("eth")
        eth_dissector:call(buffer(0):tvb(), info, ktun_tree)

    end

    local udp_table = DissectorTable.get("udp.port")
    udp_table:add(60000, ktun_proto)
end
