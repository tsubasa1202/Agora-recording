const log4js = require('log4js');
const logger = exports = module.exports = {};
log4js.configure({
     appenders: [
         {
             "type": "file",
             "category": "request",
             "filename": "./logs/request.log",
             "pattern": "-yyyy-MM-dd"
         }
    ]
});

logger.request = log4js.getLogger('request');