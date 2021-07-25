package main

import (
	"net"
	"sync"

	"time"

	dns "github.com/miekg/dns"
	rc "github.com/paulbellamy/ratecounter"
)

type (
	Conn struct {
		in    chan []byte
		out   chan []byte
		close chan bool
	}

	KCPConn struct {
		net.Conn
		localId     [4]byte
		remoteId    [4]byte
		initialized bool
		new_sent    bool
	}

	Listener struct {
		Listener net.Listener
		refcnt   int
	}

	ListenerProtocol int
	BindRequestType  int

	PortMap struct {
		From int
		To   int
	}

	BindRequestHeader struct {
		Protocol ListenerProtocol `msgpack:"prot"`
		BindInfo string           `msgpack:"bind"`
		Timeout  int              `msgpack:"timeout"`
		MTU      int              `msgpack:"mtu"`
	}

	DNSRequest struct {
		Name string
		Type string
		IPs  chan []string
	}

	DNSCacheRecord struct {
		ResponseRecords []dns.RR
		LastActivity    time.Time
	}

	ConnectionAcceptHeader struct {
		Extra      bool   `msgpack:"extra"`
		LocalHost  string `msgpack:"lhost"`
		LocalPort  int    `msgpack:"lport"`
		RemoteHost string `msgpack:"rhost"`
		RemotePort int    `msgpack:"rport"`
		Error      string `msgpack:"error"`
	}

	DNSListener struct {
		Conn net.Conn

		Domain string

		DNSCache    map[string]*DNSCacheRecord
		UDPServer   *dns.Server
		TCPServer   *dns.Server
		DNSRequests chan *DNSRequest

		processedRequests sync.WaitGroup

		dnsRequestsCounter          *rc.RateCounter
		dnsRemoteRequestsCounter    *rc.RateCounter
		dnsProcessedRequestsCounter *rc.RateCounter

		cacheLock sync.Mutex

		activeLock sync.Mutex
		active     bool

		pendingRequests int32
	}

	Daemon struct {
		Addr string

		DNSLock     sync.Mutex
		DNSCheck    sync.Mutex
		DNSListener *DNSListener

		Listeners     map[int]*Listener
		ListenersLock sync.Mutex

		UsersCount int32
	}

	IPInfo struct {
		IP string `msgpack:"ip"`
	}

	Extra struct {
		Extra bool   `msgpack:"extra"`
		Data  string `msgpack:"data"`
	}

	TLSAcceptorConfig struct {
		CACert string `msgpack:"ca"`
		Cert   string `msgpack:"cert"`
		Key    string `msgpack:"key"`
	}

	KeepAlive struct {
		Tick int64 `msgpack:"keepalive"`
		Last bool  `msgpack:"last"`
	}

	NetReader struct {
		mtu  int
		in   net.Conn
		out  net.Conn
		err  error
		wait chan error
	}

	NetForwarder struct {
		pproxy net.Conn
		remote net.Conn
	}
)

const (
	INFO ListenerProtocol = 0
	DNS  ListenerProtocol = iota
	TCP  ListenerProtocol = iota
	KCP  ListenerProtocol = iota
	TLS  ListenerProtocol = iota

	KCP_NEW = 0x0
	KCP_DAT = 0x1
	KCP_END = 0x2
)
