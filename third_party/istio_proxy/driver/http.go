// Copyright 2019 Istio Authors
// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package driver

import (
  "io"
  "strings"
	"istio.io/proxy/test/envoye2e/driver"

	"fmt"
	"io/ioutil"
	"log"
	"net/http"
	"net/http/httputil"
	"time"
)

const (
	DefaultTimeout = 10 * time.Second
	None           = "-"
	Any            = "*"
)

// HTTPCall sends a HTTP request to a localhost port, and then check the response code, and response headers.
type HTTPCall struct {
	// Method
	Method string
	// URL path
	Path string
	// Port specifies the port in 127.0.0.1:PORT
	Port uint16
	// Body to send with the request, only supported with POST method
	RequestBody string
	// Body is the expected body
	Body string
	// RequestHeaders to send with the request
	RequestHeaders map[string]string
	// ResponseCode to expect
	ResponseCode int
	// ResponseHeaders to expect
	ResponseHeaders map[string]string
	// Timeout (must be set to avoid the default)
	Timeout time.Duration
}

func Get(port uint16, body string) driver.Step {
	return &HTTPCall{
		Method: http.MethodGet,
		Port:   port,
		Body:   body,
	}
}

func (g *HTTPCall) Run(_ *driver.Params) error {
	url := fmt.Sprintf("http://127.0.0.1:%d%v", g.Port, g.Path)
	if g.Timeout == 0 {
		g.Timeout = DefaultTimeout
	}
	var reader io.Reader
	if g.Method == http.MethodPost {
	  reader = strings.NewReader(g.RequestBody)
	} else {
	  reader = nil
	}
	req, err := http.NewRequest(g.Method, url, reader)
	if err != nil {
		return err
	}
	for key, val := range g.RequestHeaders {
		req.Header.Add(key, val)
	}
	dump, _ := httputil.DumpRequest(req, false)
	log.Printf("HTTP request:\n%s", string(dump))

	client := &http.Client{Timeout: g.Timeout}
	resp, err := client.Do(req)
	if err != nil {
		return err
	}
	code := resp.StatusCode
	wantCode := 200
	if g.ResponseCode != 0 {
		wantCode = g.ResponseCode
	}
	if code != wantCode {
		return fmt.Errorf("error code for :%d: %d", g.Port, code)
	}

	bodyBytes, err := ioutil.ReadAll(resp.Body)
	defer resp.Body.Close()
	if err != nil {
		return err
	}
	body := string(bodyBytes)
	if g.Body != "" && g.Body != body {
		return fmt.Errorf("got body %q, want %q", body, g.Body)
	}

	for key, val := range g.ResponseHeaders {
		got := resp.Header.Get(key)
		switch val {
		case Any:
			if got == "" {
				return fmt.Errorf("got response header %q, want any", got)
			}
		case None:
			if got != "" {
				return fmt.Errorf("got response header %q, want none", got)
			}
		default:
			if got != val {
				return fmt.Errorf("got response header %q, want %q", got, val)
			}
		}
	}

	return nil
}

func (g *HTTPCall) Cleanup() {}
