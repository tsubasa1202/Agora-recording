const express = require('express')
const app = express()
const port = 8080
// const RecordManager = require('./recordManager')
const bodyParser = require('body-parser')
const { exec } = require('child_process')

app.use(bodyParser.json());

app.post('/recorder/v1/start', (req, res, next) => {
    let { body } = req;
    let { appid, channel, key } = body;
    if (!appid) {
        throw new Error("appid is mandatory");
    }
    if (!channel) {
        throw new Error("channel is mandatory");
    }
    
    const recordStartCommand = `./recorder_local --appId ${appid}  --uid 0 --channel ${channel}  --recordFileRootDir ~/withlive-agora-recording/server/output --appliteDir  ~/withlive-agora-recording/bin  --idle=10 --isMixingEnabled=1 --layoutMode=1`
    exec(recordStartCommand, (error, stdout, stderr) => {
        if (error || stderr) {
            const err = error ? error : stderr
            console.error(`[ERROR] ${err}`)
            res.status(500).json({
                success: false
            })
            return
        }
        //start recorder success
        res.status(200).json({
            success: true
        })
    })
    /*
    RecordManager.start(key, appid, channel).then(recorder => {
        //start recorder success
        res.status(200).json({
            success: true,
            sid: recorder.sid
        });
    }).catch((e) => {
        //start recorder failed
        next(e);
    });
    */
})

app.post('/recorder/v1/stop', (req, res, next) => {
    let { body } = req;
    let { sid } = body;
    if (!sid) {
        throw new Error("sid is mandatory");
    }

    /*
    RecordManager.stop(sid);
    res.status(200).json({
        success: true
    });
    */
})

app.use( (err, req, res, next) => {
    console.error(err.stack)
    res.status(500).json({
        success: false,
        err: err.message || 'generic error'
    })
})

app.listen(port)
