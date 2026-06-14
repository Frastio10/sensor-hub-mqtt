import type { Device } from '../types'

const BASE = 'http://localhost:8080'

export async function getDevices(): Promise<Device[]> {
  const res = await fetch(`${BASE}/api/devices`)
  if (!res.ok) throw new Error(`HTTP ${res.status}`)
  return res.json()
}

export async function sendCommand(
  id: string,
  component: string,
  status: number,
) {
  const res = await fetch(`${BASE}/api/devices/${id}/command/${component}`, {
    method: 'POST',
    body: JSON.stringify({ status }),
  })
  if (!res.ok) throw new Error(`HTTP ${res.status}`)
}
