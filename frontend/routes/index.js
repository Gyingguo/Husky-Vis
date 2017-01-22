var promise = require("bluebird");
var express = require('express');
var router = express.Router();

var connect = require('./connect/connect');
var encoding = require('./encoding/encoding');

function colloctAttributes(attributes) {
  var result = [];

  for (var i = 0; i < attributes.length; i++) {
    result.push(attributes[i].attribute);
  }

  return result;
}
// handle suggestions
function handleSuggestions(suggestions) {
  var result = [];

  for (var i = 0; i < suggestions.length; i++) {
    var chartType = (suggestions[i].chart_type + '').substr(4);
    var encodingValue = encoding.encoding(suggestions[i], chartType);
    result.push(encodingValue);
  }

  return result;;
}

/* GET home page. */
router.get('/', function(req, res, next) {

  var result = {
    "title": "Husky-Visualization",
    "data": {
      "attributes": [],
      "selectedVis": [],
      "recommendedVis": []
    }
  };

  // promisify all
  promise.promisifyAll(connect);
  // connect to c++ backend
  connect.init();

  connect.get_attributesAsync().then(function(response) {
      // get the attributes
      result.data.attributes = colloctAttributes(response);

      connect.get_suggestionsAsync().then(function(response) {
        // get topksuggestions visualization result
        result.data.recommendedVis = handleSuggestions(response);
        // response frontend
        res.render('main', result);

        connect.end();
      }).catch(function(err) {
        console.log(err);
      });

  }).catch(function(err) {
    console.log(err);
  });

});


module.exports = router;
