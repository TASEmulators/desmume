class MyAudioWorklet extends AudioWorkletProcessor {
    constructor() {
        super()
        this.FIFO_CAP = 2048*2
        this.fifo = new Int16Array(this.FIFO_CAP)
        this.fifoHead = 0
        this.fifoLen = 0
        this.port.onmessage = (e) => {
            var buf = e.data
            //console.log(buf)
            for (var i = 0; i < buf.length; i+=2) {
                if (this.fifoLen + 2 > this.FIFO_CAP) {
                    //console.log("overflow")
                    break
                }
                this.fifoEnqueue(buf[i])
                this.fifoEnqueue(buf[i + 1])
            }
        }
    }

    fifoDequeue() {
        var ret = this.fifo[this.fifoHead]
        this.fifoHead += 1
        this.fifoHead %= this.FIFO_CAP
        this.fifoLen -= 1
        return ret
    }

    fifoEnqueue(v) {
        this.fifo[(this.fifoHead + this.fifoLen) % this.FIFO_CAP] = v
        this.fifoLen += 1
    }

    process(inputs, outputs, parameters) {
        const output = outputs[0]
        const chan0 = output[0]
        const chan1 = output[1]

        for (var i = 0; i < chan0.length; i++) {
            if (this.fifoLen < 2) {
                //console.log("underflow")
                break
            }
            chan0[i] = this.fifoDequeue() / 32768.0
            chan1[i] = this.fifoDequeue() / 32768.0
        }
        return true
    }
}

registerProcessor('my-worklet', MyAudioWorklet)