# Deploy RaftKeeper

RaftKeeper can be deployed on Linux and macOS operating systems. The following instructions 
demonstrate how to deploy a standalone node and a 3-node cluster.

### Deploy a standalone node

1. Download binary file from [here](https://github.com/JDRaftKeeper/RaftKeeper/releases).
Please note that if you want to use RaftKeeper in ClickHouse, choose the installation package whose name contains 'clickhouse'.
Because ClickHouse from v22.10 is a little incompatible with Zookeeper, and here we provide 2 installation packages.

2. Uncompress the file using the following command:
```
tar -xzvf RaftKeeper-xxx.tar.gz
```

3. Update `conf/config.xml`
Set `my_id` for your node and set a directory, let's assume is `/path/to/raftkeeper/root/dir`, for your data.
You can just copy the following text into `conf/config.xml`

```xml
<?xml version="1.0"?>
<raftkeeper>
    <logger>
        <path>/path/to/raftkeeper/root/dir/log/raftkeeper.log</path>
        <err_log_path>/path/to/raftkeeper/root/dir/log/raftkeeper.err.log</err_log_path>
    </logger>
    <keeper>
        <my_id>1</my_id>
        <log_dir>/path/to/raftkeeper/root/dir//data/log</log_dir>
        <snapshot_dir>/path/to/raftkeeper/root/dir/data/snapshot</snapshot_dir>
    </keeper>
</raftkeeper>
```

4. Start the node
```
cd RaftKeeper/bin && sh start.sh
```

### Deploy a 3-nodes cluster

1. Download binary file from [here](https://github.com/JDRaftKeeper/RaftKeeper/releases).
Please choose the correct installation package.

2. Uncompress the file using the following command:
```
tar -xzvf RaftKeeper-xxx.tar.gz
```

3. Update the `conf/config.xml` file for each node in the cluster:

The difference than deploying a standalone node is that you should configure the cluster for each node.
The following demonstrates how to configure the nodes.

Node 1:
```xml
<?xml version="1.0"?>
<raftkeeper>
    <logger>
        <path>/path/to/raftkeeper/root/dir/log/raftkeeper.log</path>
        <err_log_path>/path/to/raftkeeper/root/dir/log/raftkeeper.err.log</err_log_path>
    </logger>
    <keeper>
        <my_id>1</my_id>
        <log_dir>/path/to/raftkeeper/root/dir//data/log</log_dir>
        <snapshot_dir>/path/to/raftkeeper/root/dir/data/snapshot</snapshot_dir>
        <cluster>
            <server>
                <id>1</id>
                <host>ip_of_node1</host>
            </server>
            <server>
                <id>2</id>
                <host>ip_of_node2</host>
            </server>
            <server>
                <id>3</id>
                <host>ip_of_node3</host>
            </server>
        </cluster>
    </keeper>
</raftkeeper>
```

Node 2:
```xml
<?xml version="1.0"?>
<raftkeeper>
    <logger>
        <path>/path/to/raftkeeper/root/dir/log/raftkeeper.log</path>
        <err_log_path>/path/to/raftkeeper/root/dir/log/raftkeeper.err.log</err_log_path>
    </logger>
    <keeper>
        <my_id>2</my_id>
        <log_dir>/path/to/raftkeeper/root/dir//data/log</log_dir>
        <snapshot_dir>/path/to/raftkeeper/root/dir/data/snapshot</snapshot_dir>
        <cluster>
            <server>
                <id>1</id>
                <host>ip_of_node1</host>
            </server>
            <server>
                <id>2</id>
                <host>ip_of_node2</host>
            </server>
            <server>
                <id>3</id>
                <host>ip_of_node3</host>
            </server>
        </cluster>
    </keeper>
</raftkeeper>
```

Node 3:
```xml
<?xml version="1.0"?>
<raftkeeper>
    <logger>
        <path>/path/to/raftkeeper/root/dir/log/raftkeeper.log</path>
        <err_log_path>/path/to/raftkeeper/root/dir/log/raftkeeper.err.log</err_log_path>
    </logger>
    <keeper>
        <my_id>3</my_id>
        <log_dir>/path/to/raftkeeper/root/dir/data/log</log_dir>
        <snapshot_dir>/path/to/raftkeeper/root/dir/data/snapshot</snapshot_dir>
        <cluster>
            <server>
                <id>1</id>
                <host>ip_of_node1</host>
            </server>
            <server>
                <id>2</id>
                <host>ip_of_node2</host>
            </server>
            <server>
                <id>3</id>
                <host>ip_of_node3</host>
            </server>
        </cluster>
    </keeper>
</raftkeeper>
```
Please note that you need to replace `ip_of_node1`, `ip_of_node2`, and `ip_of_node3` with the respective IP addresses of the nodes in the cluster
and `/path/to/raftkeeper/root/dir/` with your RaftKeeper root directory.


4. Start the cluster by executing the following command in every node:
```
cd RaftKeeper/bin && sh start.sh
```

### Deploy with learner nodes

RaftKeeper also support learner node which will not participate in leader election or log replication.
The following demonstrates how to configure the 4th node as learner node.
```xml
    <cluster>
        <server>
            <id>1</id>
            <host>ip_of_node1</host>
        </server>
        <server>
            <id>2</id>
            <host>ip_of_node2</host>
        </server>
        <server>
            <id>3</id>
            <host>ip_of_node3</host>
        </server>
        <server>
            <id>4</id>
            <host>ip_of_node4</host>
            <learner>true</learner>
        </server>
    </cluster>
```