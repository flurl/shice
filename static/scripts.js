

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


let confirmLinks = document.querySelectorAll('.image_confirm_link');
confirmLinks.forEach((link) => {
    link.addEventListener('click', (ev) => {
        ev.preventDefault();
        
        const url = link.getAttribute('href'); // Get the URL from the link's href attribute

        fetch(url, {
            method: 'GET',
            headers: {
                'Accept': 'application/json'
            }
        })
        .then(response => {
            if (!response.ok) {
                throw new Error('Network response was not ok');
            }
            
            const file = new URLSearchParams(url.split('?')[1]).get('file');
            document.querySelector(`img[src='${file}']`).classList.add('prepared');
        })
        .catch(error => {
            console.error('There was a problem with the fetch operation:', error);
        });
    });
});


let deleteLinks = document.querySelectorAll('.image_delete_link');
deleteLinks.forEach((link) => {
    link.addEventListener('click', (ev) => {
        ev.preventDefault();
        
        if (confirm('Are you sure that you want to delete this file?')) {
            
            const url = link.getAttribute('href'); // Get the URL from the link's href attribute

            fetch(url, {
                method: 'GET',
                headers: {
                    'Accept': 'application/json'
                }
            })
            .then(response => {
                if (!response.ok) {
                    throw new Error('Network response was not ok');
                }
                
                const file = new URLSearchParams(url.split('?')[1]).get('file');
                document.querySelector(`img[src='${file}']`).classList.add('deleted');
            })
            .catch(error => {
                console.error('There was a problem with the fetch operation:', error);
            });
        }
    });
});

