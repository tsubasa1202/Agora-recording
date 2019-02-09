const express = require('express')
const app = express()
const port = 8080
// const RecordManager = require('./recordManager')
const bodyParser = require('body-parser')
// const { exec } = require('child_process')
const childProcess = require('child_process')
const log4js = require('log4js')
log4js.configure({
  appenders: { cheese: { type: 'file', filename: 'access.log' } },
  categories: { default: { appenders: ['withlive'], level: 'info' } }
});

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

    logger.info(`channel: ${channel}`)
    const process = childProcess.spawn('/home/tsubasa/withlive-agora-recording/samples/cpp/recorder_local', ['--appId', appid, '--uid',  0,  '--channel', channel,  '--recordFileRootDir',  '/home/tsubasa/withlive-agora-recording/server/output',  '--appliteDir', '/home/tsubasa/withlive-agora-recording/bin', '--idle=10', '--isMixingEnabled=1', '--layoutMode=1'])
    
        process.stdout.on('data', (data) => {
            // console.log(`stdout: ${data}`)
            logger.info(`stdout: ${data}`)
            const containSuccessJoin = String(data).indexOf(`join channel Id: ${channel}, with uid:`) !== -1
            if(containSuccessJoin){
                res.status(200).json({
                    success: true,
                    message: String(data)
                })
            }
        })
        
        process.stderr.on('data', (data) => {
            // console.error(`stderr: ${data}`)
            logger.error(`stderr: ${data}`)
        })
        
        process.on('close', (code) => {
            // console.log(`child process exited with code ${code}`)
            logger.info(`child process exited with code ${code}`)
        })
})

app.use( (err, req, res, next) => {
   // console.error(err.stack)
   logger.error(err.stack)
    res.status(500).json({
        success: false,
        err: err.message || 'generic error'
    })
})

app.listen(port)
