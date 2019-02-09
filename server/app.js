const express = require('express')
const app = express()
const port = 80
// const RecordManager = require('./recordManager')
const bodyParser = require('body-parser')
// const { exec } = require('child_process')
const childProcess = require('child_process');

app.use(bodyParser.json());

app.post('/recorder/v1/start', (req, res, next) => {
    console.log('/recorder/v1/start')
    let { body } = req;
    let { appid, channel, key } = body;
    if (!appid) {
        throw new Error("appid is mandatory");
    }
    if (!channel) {
        throw new Error("channel is mandatory");
    }
    
    // const recordStartCommand = `/home/oshima/withlive-agora-recording/samples/cpp/recorder_local --appId ${appid} --uid 0 --channel ${channel} --recordFileRootDir /home/oshima/withlive-agora-recording/server/output --appliteDir  /home/oshima/withlive-agora-recording/bin  --idle=10 --isMixingEnabled=1 --layoutMode=1 &`
    const process = childProcess.spawn('/home/oshima/withlive-agora-recording/samples/cpp/recorder_local', ['--appId', appid, '--uid',  0,  '--channel', channel,  '--recordFileRootDir',  '/home/oshima/withlive-agora-recording/server/output',  '--appliteDir', '/home/oshima/withlive-agora-recording/bin', '--idle=10', '--isMixingEnabled=1', '--layoutMode=1'])
    
        process.stdout.on('data', (data) => {
            console.log(`stdout: ${data}`)
            const containSuccessJoin = String(data).indexOf(`join channel Id: ${channel}, with uid:`) !== -1
            if(containSuccessJoin){
                res.status(200).json({
                    success: true,
                    message: String(data)
                })
            }
        })
        
        process.stderr.on('data', (data) => {
            console.log(`stderr: ${data}`)
        })
        
        process.on('close', (code) => {
            console.log(`child process exited with code ${code}`)
        })



    /*
    const process = childProcess.execFile('/home/oshima/withlive-agora-recording/samples/cpp/recorder_local', 
    ['--appId',appid, '--uid',  0,  '--channel', channel,  '--recordFileRootDir',
    '/home/oshima/withlive-agora-recording/server/output',  '--appliteDir',
    '/home/oshima/withlive-agora-recording/bin', '--idle=10', '--isMixingEnabled=1', '--layoutMode=1', '&'], (error, stdout, stderr) => {
        
        if (error) {
            console.error("stderr", stderr)
            return res.status(500).json({
                success: false,
                pid: process.pid
            })
        }
        // console.log(stdout);
        // console.log("pid: " +process.pid)
        return res.status(200).json({
            success: true,
            pid: process.pid
        })
    })
    */
    
    
    /*
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
    */
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

/*
app.post('/recorder/v1/stop', (req, res, next) => {
    let { body } = req;
    let { sid } = body;
    if (!sid) {
        throw new Error("sid is mandatory");
    }

    RecordManager.stop(sid);
    res.status(200).json({
        success: true
    });
})
*/

app.use( (err, req, res, next) => {
    console.error(err.stack)
    res.status(500).json({
        success: false,
        err: err.message || 'generic error'
    })
})

app.listen(port)
