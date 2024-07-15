

let checkboxes = document.querySelectorAll('input[type=checkbox][name=file]');
checkboxes.forEach(function(checkbox) {
    checkbox.addEventListener('click', function(ev) {
        // This function will be called when the checkbox is clicked
        console.log('Checkbox clicked:', this.checked);

        // You can add additional logic or functionality here
        // For example, you can perform actions based on whether the checkbox is checked or unchecked
        if (this.checked) {
            // Do something when the checkbox is checked
            console.log('Checkbox is checked');
            if (ev.shiftKey) {
                console.log("shift key pressed");
                selectMultiple(ev.target);
            }
            lastSelectedCheckbox = ev.target;
        } else {
          // Do something when the checkbox is unchecked
          console.log('Checkbox is unchecked');
        }
    });
});

let lastSelectedCheckbox = checkboxes[0];
const selectMultiple = function(clickedCheckbox) {
    let doSelect = false;
    checkboxes.forEach(cb => {
        if (doSelect && (cb === lastSelectedCheckbox || cb === clickedCheckbox)) {
            doSelect = false;
            return;
        }
        if (!doSelect && (cb === lastSelectedCheckbox || cb === clickedCheckbox)) {
            doSelect = true;
        }
        if (doSelect) {
            cb.checked = true;
        }
    });
}


function handleFetch(url, successCallback) {
    return fetch(url, {
        method: 'GET',
        headers: {
            'Accept': 'application/json'
        }
    })
    .then(response => {
        if (!response.ok) {
            throw new Error('Network response was not ok');
        }
        return successCallback();
    })
    .catch(error => {
        console.error('There was a problem with the fetch operation:', error);
    });
}

function addClickListener(selector, clickHandler) {
    document.querySelectorAll(selector).forEach(link => {
        link.addEventListener('click', clickHandler);
    });
}

addClickListener('.image_confirm_link', (ev) => {
    ev.preventDefault();
    const url = ev.target.getAttribute('href');
    handleFetch(url, () => {
        const file = new URLSearchParams(url.split('?')[1]).get('file');
        document.querySelector(`img[src='${file}']`).classList.add('prepared');
    });
});

addClickListener('.image_delete_link', (ev) => {
    ev.preventDefault();
    if (confirm('Are you sure that you want to delete this file?')) {
        const url = ev.target.getAttribute('href');
        handleFetch(url, () => {
            const file = new URLSearchParams(url.split('?')[1]).get('file');
            document.querySelector(`img[src='${file}']`).classList.add('deleted');
        });
    }
});

addClickListener('.image-tag-link', (ev) => {
    ev.preventDefault();
    const url = ev.target.getAttribute('href');
    handleFetch(url, () => {
        const td = ev.target.parentElement;
        const scoreElements = td.parentElement.parentElement.querySelectorAll('tr td.score');
        
        scoreElements.forEach(ele => {
            ele.innerHTML = '0.0';
            ele.style.backgroundColor = 'hsl(100.0, 100%, 50%)';
        });

        const selectedScore = td.previousElementSibling;
        selectedScore.innerHTML = '1.0';
        selectedScore.style.backgroundColor = 'hsl(400.0, 100%, 50%)';

        const tagElement = ev.target.closest('div.image-and-scores').querySelector('.image_overlay_tag');
        tagElement.innerHTML = td.previousElementSibling.previousElementSibling.innerHTML;
    });
});
