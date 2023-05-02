var dark = false;
const urlMap = dark ? 'https://tiles.stadiamaps.com/styles/alidade_smooth_dark.json' : 'https://tiles.stadiamaps.com/styles/alidade_smooth.json';

const map = new maplibregl.Map({
    container: 'map',
    style: urlMap,  // Style URL; see our documentation for more options
    center: [-6.375807, 39.473168],  // Initial focus coordinate
    zoom: 12
});

// MapLibre GL JS does not handle RTL text by default, so we recommend adding this dependency to fully support RTL rendering. 
maplibregl.setRTLTextPlugin('https://api.mapbox.com/mapbox-gl-js/plugins/mapbox-gl-rtl-text/v0.2.1/mapbox-gl-rtl-text.js');

const url = '/query';
map.on('load', function () {
    /*var request = new XMLHttpRequest();
    window.setInterval(function () {
        // make a GET request to parse the GeoJSON at the url
        request.open('GET', url, true);
        request.onload = function () {
            if (this.status >= 200 && this.status < 400) {
                // retrieve the JSON from the response
                var json = JSON.parse(this.response);

                // update the drone symbol's location on the map
                map.getSource('gasses').setData(json);
            }
        };
        request.send();
        alert("Request sent");
    }, 10000);*/

    
    // Add a geojson point source.
    // Heatmap layers also work with a vector tile source.
    map.addSource('gasses', {
        'type': 'geojson',
        'data': url
    });


    map.addLayer(
        {
            'id': 'gasses-heat',
            'type': 'heatmap',
            'source': 'gasses',
            //'maxzoom': 9,
            'paint': {
                // Increase the heatmap weight based on frequency and property magnitude
                'heatmap-weight': [
                    'interpolate',
                    ['linear'],
                    ['get', 'mag'],
                    0,
                    0,
                    4,
                    1
                ],
                /*// Increase the heatmap color weight weight by zoom level
                // heatmap-intensity is a multiplier on top of heatmap-weight
                'heatmap-intensity': [
                    'interpolate',
                    ['linear'],
                    ['zoom'],
                    0,
                    1,
                    15,
                    3
                ],*/
                // Color ramp for heatmap.  Domain is 0 (low) to 1 (high).
                // Begin color ramp at 0-stop with a 0-transparancy color
                // to create a blur-like effect.
                'heatmap-color': [
                    'interpolate',
                    ['linear'],
                    ['heatmap-density'],
                    0,
                    'rgba(0, 235, 23, 0)',
                    0.25,
                    'rgb(235, 235, 2)',
                    0.5,
                    'rgb(235, 157, 2)',
                    0.75,
                    'rgb(235, 2, 2)',
                    1,
                    'rgb(163, 2, 69)'
                ],
                // Adjust the heatmap radius by zoom level
                'heatmap-radius': [
                    'interpolate',
                    ['linear'],
                    ['zoom'],
                    0,
                    2,
                    15,
                    100
                ],
                /*// Transition from heatmap to circle layer by zoom level
                'heatmap-opacity': [
                    'interpolate',
                    ['linear'],
                    ['zoom'],
                    7,
                    1,
                    15,
                    0
                ]*/
            }
        },
        'waterway'
    );

    map.addLayer(
        {
            'id': 'gasses-point',
            'type': 'circle',
            'source': 'gasses',
            'minzoom': 7,
            'paint': {
                /*// Size circle radius by earthquake magnitude and zoom level
                'circle-radius': [
                    'interpolate',
                    ['linear'],
                    ['zoom'],
                    7,
                    ['interpolate', ['linear'], ['get', 'LVL'], 1, 1, 6, 4],
                    16,
                    ['interpolate', ['linear'], ['get', 'LVL'], 1, 5, 6, 50]
                ],*/
                'circle-radius': 15,
                // Color circle by earthquake magnitude
                'circle-color': [
                    'interpolate',
                    ['linear'],
                    ['get', 'LVL'],
                    0,
                    'rgb(0, 235, 23)',
                    1,
                    'rgb(235, 235, 2)',
                    2,
                    'rgb(235, 157, 2)',
                    3,
                    'rgb(235, 2, 2)',
                    4,
                    'rgb(163, 2, 69)'
                ],
                'circle-stroke-color': 'white',
                'circle-stroke-width': 1,
                /*// Transition from heatmap to circle layer by zoom level
                'circle-opacity': [
                    'interpolate',
                    ['linear'],
                    ['zoom'],
                    7,
                    0,
                    8,
                    1
                ]*/
            }
        },
        'waterway'
    );

    // When a click event occurs on a feature in the places layer, open a popup at the
    // location of the feature, with description HTML from its properties.
    map.on('click', 'gasses-point', function (e) {
        let coordinates = e.features[0].geometry.coordinates.slice();
            
        // Ensure that if the map is zoomed out such that multiple
        // copies of the feature are visible, the popup appears
        // over the copy being pointed to.
        while (Math.abs(e.lngLat.lng - coordinates[0]) > 180) {
            coordinates[0] += e.lngLat.lng > coordinates[0] ? 360 : -360;
        }
            
        new maplibregl.Popup()
            .setLngLat(coordinates)
            .setHTML("Hello world")
            .addTo(map);
    });
        
    // Change the cursor to a pointer when the mouse is over the places layer.
    map.on('mouseenter', 'gasses-point', function () {
        map.getCanvas().style.cursor = 'pointer';
    });
        
    // Change it back to a pointer when it leaves.
    map.on('mouseleave', 'gasses-point', function () {
        map.getCanvas().style.cursor = '';
    });
});

// Add the navigation control
map.addControl(new maplibregl.NavigationControl());