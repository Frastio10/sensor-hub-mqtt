package main

import (
	"encoding/json"
	"fmt"
	"net/http"
	"sync"

	mqtt "github.com/eclipse/paho.mqtt.golang"
)

const (
	broker   = "tcp://localhost:1883"
	clientID = "go-mqtt-client"
	topic    = "devices/#"
)

type Device struct {
	DeviceID string `json:"device_id"`
	MAC      string `json:"mac"`
	IP       string `json:"ip"`
	SSID     string `json:"ssid"`
}

var (
	devices = make(map[string]*Device)
	mu      sync.RWMutex
)

var connectHandler mqtt.OnConnectHandler = func(client mqtt.Client) {
	fmt.Println("connected to mqtt")

	if token := client.Subscribe(topic, 0, onMessageReceived); token.Wait() && token.Error() != nil {
		panic(token.Error())

	}
}

var connectLostHandler mqtt.ConnectionLostHandler = func(client mqtt.Client, err error) {
	fmt.Printf("disconnected: %v", err)
}

func onMessageReceived(client mqtt.Client, message mqtt.Message) {
	fmt.Printf("Received message on topic: %s\nMessage: %s\n", message.Topic(), message.Payload())

	var device Device
	err := json.Unmarshal(message.Payload(), &device)
	if err != nil {
		fmt.Println("Error:", err)
		return
	}

	mu.Lock()
	devices[device.DeviceID] = &device
	mu.Unlock()

	fmt.Printf("Parsed Struct: %+v\n", device)
}

func devicesHandler(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "application/json")

	mu.RLock()
	devicesArr := make([]*Device, 0, len(devices))

	for _, device := range devices {
		devicesArr = append(devicesArr, device)
	}

	mu.RUnlock()

	json.NewEncoder(w).Encode(devicesArr)
}

func cors(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Access-Control-Allow-Origin", "*")
		w.Header().Set("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS")
		w.Header().Set("Access-Control-Allow-Headers", "Content-Type, Authorization")

		if r.Method == "OPTIONS" {
			w.WriteHeader(http.StatusOK)
			return
		}

		next.ServeHTTP(w, r)
	})
}

func main() {
	opts := mqtt.NewClientOptions()
	opts.AddBroker(broker)
	opts.SetClientID(clientID)
	opts.OnConnect = connectHandler
	opts.OnConnectionLost = connectLostHandler

	client := mqtt.NewClient(opts)
	if token := client.Connect(); token.Wait() && token.Error() != nil {
		panic(token.Error())
	}

	mux := http.NewServeMux()
	mux.HandleFunc("/api/devices", devicesHandler)

	fmt.Println("SERVER :8080...")

	if err := http.ListenAndServe(":8080", cors(mux)); err != nil {
		panic(err)
	}
}
