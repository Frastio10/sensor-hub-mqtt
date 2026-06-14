export const pad = (n: number) => n.toString().padStart(2, '0')

export function clock(d: Date | null) {
  if (!d) return '--:--:--'
  return `${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())}`
}
