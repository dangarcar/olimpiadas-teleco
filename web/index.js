const levels = {
    1 : 'Óptima',
    2 : 'Buena',
    3 : 'Regular',
    4 : 'Mala',
    5 : 'Muy mala'
}
const levelColors = {
    1 : 'rgb(0, 235, 23)',
    2 : 'rgb(235, 235, 2)',
    3 : 'rgb(235, 157, 2)',
    4 : 'rgb(235, 2, 2)',
    5 : 'rgb(163, 2, 69)'
}

let dark = false;
const urlMap = dark ? 'https://tiles.stadiamaps.com/styles/alidade_smooth_dark.json' : 'https://tiles.stadiamaps.com/styles/alidade_smooth.json';
const url = '/data';

const map = new maplibregl.Map({
    container: 'map',
    style: urlMap,  // Style URL; see our documentation for more options
    center: [-6.375807, 39.473168],  // Initial focus coordinate
    zoom: 12
});

map.on('load', () => {
    // Add a geojson point source.
    map.addSource('gasses', {
        'type': 'geojson',
        'data': url
    });

    map.addLayer({
            'id': 'gasses-point',
            'type': 'circle',
            'source': 'gasses',
            'minzoom': 1,
            'paint': {
                'circle-radius': 15,
                'circle-color': [
                'interpolate',
                    ['linear'],
                    ['get', 'LVL'],
                    1,
                    'rgb(0, 235, 23)',
                    2,
                    'rgb(235, 235, 2)',
                    3,
                    'rgb(235, 157, 2)',
                    4,
                    'rgb(235, 2, 2)',
                    5,
                    'rgb(163, 2, 69)'
                ],
                'circle-stroke-color': 'white',
                'circle-stroke-width': 1,
            }
        },
        'waterway'
    );

    // When a click event occurs on a feature in the places layer, open a popup at the location of the feature, with description HTML from its properties.
    map.on('click', 'gasses-point', async (e) => {
        CloseDetails();
        let coordinates = e.features[0].geometry.coordinates.slice();
        let props = e.features[0].properties;

        while (Math.abs(e.lngLat.lng - coordinates[0]) > 180) {
            coordinates[0] += e.lngLat.lng > coordinates[0] ? 360 : -360;
        }

        let detail = await CreateDetails(document.getElementById("detail"), props, coordinates);
        detail.style.display = "block";
    });
        
    // Change the cursor to a pointer when the mouse is over the places layer.
    map.on('mouseenter', 'gasses-point', () => {
        map.getCanvas().style.cursor = 'pointer';
    });
        
    // Change it back to a pointer when it leaves.
    map.on('mouseleave', 'gasses-point', () => {
        map.getCanvas().style.cursor = '';
    });

    map.on('idle', () => {
        map.moveLayer('gasses-point');
    });

    //Create the legend of the map
    const legend = document.getElementById("legend");
    legend.appendChild(CreateLegend());
});

// Add the navigation control
map.addControl(new maplibregl.NavigationControl());
// MapLibre GL JS does not handle RTL text by default, so we recommend adding this dependency to fully support RTL rendering. 
maplibregl.setRTLTextPlugin('https://api.mapbox.com/mapbox-gl-js/plugins/mapbox-gl-rtl-text/v0.2.1/mapbox-gl-rtl-text.js');

function CreateLegend () {
    const list = document.createElement('ul');
    
    Object.keys(levels).forEach((key) => {
        const item = document.createElement('li');
        item.innerHTML = `<span style="background-color: ${levelColors[key]}"></span>${levels[key]}`;
        list.appendChild(item);
    });
    
    return list;
}

const CloseDetails = () => {
    document.getElementById("detail").style.display = "none";
}

const CreateDetails = async (detail, props, coords) => {
    await CreateDetailLocation(detail, coords);
    await CreateDetailContent(detail, props);
    await CreateAirometer(detail, props);

    return detail;
}

const CreateDetailContent = async (detail, props) => {
    const table = detail.querySelector('.table');
    table.innerHTML = '';

    table.appendChild(createRow('Fecha', (() => {
        const date = new Date(0);
        date.setUTCSeconds(props.Time);
        return date.toLocaleString('es-ES');
    })()));

    table.appendChild(createRow('Temperatura', `${props.Temp.toFixed(2)}&deg;C`));
    table.appendChild(createRow('Humedad', `${props.Hum.toFixed(2)}%`));

    table.appendChild(createRow('NO<sub>2</sub>', `${props.NO2.toFixed(2)} ppm`));
    table.appendChild(createRow('CO', `${props.CO.toFixed(2)} ppm`));
    table.appendChild(createRow('NH<sub>3</sub>', `${props.NH3.toFixed(2)} ppm`));

    table.appendChild(createRow('PM 2.5', `${props.PM25} &micro;g/m<sup>3</sup>`));
    table.appendChild(createRow('PM 10', `${props.PM10} &micro;g/m<sup>3</sup>`));
}

const CreateDetailLocation = async (detail, coords) => {
    const detailLocation = detail.querySelector('.detail-location');
    detailLocation.innerHTML = '';

    const url = `https://geocode.maps.co/reverse?lat=${coords[1]}&lon=${coords[0]}&format=json`;
    let response = await fetch(url);
    response = await response.json();

    detailLocation.appendChild((() => {
        const h3 = document.createElement("h3");
        h3.innerHTML = response["display_name"];
        return h3;
    })());
    detailLocation.appendChild((() => {
        const p = document.createElement("p");
        const cod = `${Math.abs(coords[1].toFixed(4))}&deg;${coords[1]<0? 'S':'N'} ${Math.abs(coords[0].toFixed(4))}&deg;${coords[0]<0? 'W':'E'}`;
        p.innerHTML = cod;
        return p;
    })());
}

const CreateAirometer = async (detail, props) => {
    const airometer = detail.querySelector('#airometer');
    const lvl = props['LVL'];

    const description = airometer.querySelector('.level-description');
    description.innerHTML = `<p>${levels[lvl]}</p>`;

    const indicator = airometer.querySelector('.indicator');
    indicator.style.transform = `rotate(${(lvl-3)*36}deg)`;
}

const createRow = (th, td) => {
    const row = document.createElement('tr');
    row.appendChild((() => {
        const h = document.createElement('th');
        h.innerHTML = th;
        return h;
    })());
    row.appendChild((() => {
        const d = document.createElement('td');
        d.innerHTML = td;
        return d;
    })());

    return row;
}