package main

import (
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"strings"
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
	Online   bool   `json:"online"`

	States    map[string]int `json:"states"`
	Telemetry map[string]int `json:"telemetry"`
}

var (
	devices    = make(map[string]*Device)
	mu         sync.RWMutex
	mqttClient mqtt.Client
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
	parts := strings.Split(message.Topic(), "/")
	id := parts[1]
	category := parts[2]

	mu.Lock()
	defer mu.Unlock()
	device, ok := devices[id]
	if !ok {
		device = &Device{DeviceID: id}
		devices[id] = device
	}

	switch category {
	case "status":
		var status struct {
			Status int `json:"status"`
		}

		err := json.Unmarshal(message.Payload(), &status)
		if err != nil {
			fmt.Println("Error:", err)
			return
		}

		device.Online = status.Status != 0

	case "info":
		var info Device
		if err := json.Unmarshal(message.Payload(), &info); err == nil {
			device.MAC, device.IP, device.SSID = info.MAC, info.IP, info.SSID
		}

	case "state":
		if len(parts) < 4 {
			return
		}
		component := parts[3]

		var s struct {
			Status int `json:"status"`
		}
		if err := json.Unmarshal(message.Payload(), &s); err == nil {
			if device.States == nil {
				device.States = make(map[string]int)
			}
			device.States[component] = s.Status
		}

	case "telemetry":
		component := parts[3]

		var v struct {
			Value int `json:"value"`
		}

		if err := json.Unmarshal(message.Payload(), &v); err == nil {
			if device.Telemetry == nil {
				device.Telemetry = make(map[string]int)
			}
			device.Telemetry[component] = v.Value
		}
		println("")
	}

}

func getDevicesHandler(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "application/json")

	mu.RLock()
	devicesArr := make([]*Device, 0, len(devices))

	for _, device := range devices {
		devicesArr = append(devicesArr, device)
	}

	mu.RUnlock()

	json.NewEncoder(w).Encode(devicesArr)
}

func sendCommandsHandler(w http.ResponseWriter, r *http.Request) {
	id := r.PathValue("id")
	component := r.PathValue("component")

	body, _ := io.ReadAll(r.Body)

	topic := fmt.Sprintf("devices/%s/command/%s", id, component)
	token := mqttClient.Publish(topic, 1, false, body)
	token.Wait()
	w.WriteHeader(http.StatusAccepted)
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

	mqttClient = mqtt.NewClient(opts)
	if token := mqttClient.Connect(); token.Wait() && token.Error() != nil {
		panic(token.Error())
	}

	mux := http.NewServeMux()
	mux.HandleFunc("/api/devices", getDevicesHandler)
	mux.HandleFunc("POST /api/devices/{id}/command/{component}", sendCommandsHandler)

	fmt.Println("SERVER :8080...")

	if err := http.ListenAndServe(":8080", cors(mux)); err != nil {
		panic(err)
	}
}
