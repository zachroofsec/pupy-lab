package main

import (
	"crypto/tls"
	"net"
	"net/http"

	"time"

	"sync/atomic"

	log "github.com/sirupsen/logrus"
)

func NewDaemon(addr string) *Daemon {
	return &Daemon{
		Addr:      addr,
		Listeners: make(map[int]*Listener),
	}
}

func (d *Daemon) ListenAndServe() error {
	listener, err := net.Listen("tcp", d.Addr)
	if err != nil {
		log.Error("Listen error: ", err)
		return err
	}

	for {
		conn, err := listener.Accept()
		log.Debug("HANDLE INCOMING CONNECTION")
		if err != nil {
			log.Error("Accept error: ", err)
			return err
		}

		conn.(*net.TCPConn).SetKeepAlive(true)
		conn.(*net.TCPConn).SetKeepAlivePeriod(1 * time.Minute)
		conn.(*net.TCPConn).SetNoDelay(true)

		conn = tls.Server(conn, ListenerConfig)

		go d.handle(conn)
	}
}

func (d *Daemon) onListenerEnabled() {
	if atomic.AddInt32(&d.UsersCount, 1) == 1 && OnListenerEnabledURL != "" {
		response, err := http.Get(OnListenerEnabledURL)
		if err != nil {
			log.Error("Register failed: ", err)
		} else {
			log.Info("Register:", OnListenerEnabledURL, ": ", response.Status)
		}
	}
}

func (d *Daemon) onListenerDisabled() {
	if atomic.AddInt32(&d.UsersCount, -1) == 0 && OnListenerDisabledURL != "" {
		response, err := http.Get(OnListenerDisabledURL)
		if err != nil {
			log.Error("Register failed: ", err)
		} else {
			log.Info("Register:", OnListenerDisabledURL, ": ", response.Status)
		}
	}
}

func (d *Daemon) handle(conn net.Conn) {
	defer conn.Close()

	brh := &BindRequestHeader{}

	err := RecvMessage(conn, brh)
	if err != nil {
		log.Error("Couldn't read bind request: ", err)
		return
	}

	log.Debug(brh)

	/* Check PSK */

	log.Debug("PROTOCOL CODE: ", brh.Protocol)

	client := conn.RemoteAddr().String()

	switch brh.Protocol {
	case DNS:
		/* Check DNS Already served */

		d.DNSCheck.Lock()
		if d.DNSListener != nil {
			log.Warning(
				"Request: DNS Handler for domain:", brh.BindInfo,
				" client: ", client, " - request shutdown",
			)
			d.DNSListener.sendEmptyMessage()
			d.DNSListener.Shutdown()
		} else {
			log.Warning(
				"Request: DNS Handler for domain:", brh.BindInfo,
				" client: ", client, " - wait for availability",
			)
		}
		d.DNSCheck.Unlock()

		d.DNSLock.Lock()
		d.onListenerEnabled()
		log.Warning(
			"Request: DNS Handler for domain:", brh.BindInfo,
			" client: ", client, " - start",
		)
		d.serveDNS(conn, brh.BindInfo)
		log.Warning(
			"Request: DNS Handler for domain:", brh.BindInfo,
			" client: ", client, " - complete",
		)
		d.onListenerDisabled()
		d.DNSLock.Unlock()

	case INFO:
		var ip string

		if CheckExternalBindHostIP() {
			ip = ExternalBindHost
			if string(ip[0]) == "[" {
				ip = ip[1 : len(ip)-1]
			}

			log.Warning("Request: External IP:", ip, " (Manual)")
		} else {
			ipv4 := GetOutboundIPv4()
			ipv6 := GetOutboundIPv6()

			log.Warning(
				"Request: External IPv4:", ipv4,
				" IPv6:", ipv6, " (Auto)")

			ip = ipv4
		}

		SendMessage(conn, &IPInfo{
			IP: ip,
		})

	case TCP:
		log.Warning(
			"Request: TCP handler with port:", brh.BindInfo,
			" client: ", client, " - start",
		)
		d.onListenerEnabled()
		d.serveStream(-1, conn, brh.BindInfo, d.listenAcceptTCP)
		d.onListenerDisabled()
		log.Warning(
			"Request: TCP handler with port:", brh.BindInfo,
			" client: ", client, " - complete",
		)

	case KCP:
		log.Warning(
			"Request: KCP handler with port:", brh.BindInfo,
			" client: ", client, " MTU:", brh.MTU, " - start",
		)
		d.onListenerEnabled()
		d.serveStream(brh.MTU, conn, brh.BindInfo, d.listenAcceptKCP)
		d.onListenerDisabled()
		log.Warning(
			"Request: KCP handler with port:", brh.BindInfo,
			" client: ", client, " - complete",
		)

	case TLS:
		log.Warning(
			"Request: SSL handler with port:", brh.BindInfo,
			" client: ", client, " - start",
		)
		d.onListenerEnabled()
		d.serveStream(-1, conn, brh.BindInfo, d.listenAcceptTLS)
		d.onListenerDisabled()
		log.Warning(
			"Request: SSL handler with port:", brh.BindInfo,
			" client: ", client, " - complete",
		)

	default:
		log.Error("Unknown protocol", brh.Protocol)
	}
}
