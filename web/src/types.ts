export type Device = {
  device_id: string
  mac: string
  ip: string
  ssid: string
  online: boolean
  states: Record<string, number> | null
}
