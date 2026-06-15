package parser

import (
	"io"
	"net/url"
	"strings"

	"golang.org/x/net/html"
)

type ParsedImage struct {
	URL     string
	AltText string
}

type ParsedPage struct {
	Title   string
	RawText string
	Links   []string
	Images  []ParsedImage
}

func ParseHTML(body io.Reader, base string) (*ParsedPage, error) {
	baseURL, err := url.Parse(base)
	if err != nil {
		return nil, err
	}

	doc, err := html.Parse(body)
	if err != nil {
		return nil, err
	}

	var title string
	var textBuilder strings.Builder
	var links []string
	var images []ParsedImage

	var f func(*html.Node)
	f = func(n *html.Node) {
		if n.Type == html.ElementNode {
			switch n.Data {
			case "title":
				if n.FirstChild != nil {
					title = strings.TrimSpace(n.FirstChild.Data)
				}
			case "a":
				for _, a := range n.Attr {
					if a.Key == "href" {
						resolvedLink := resolveURL(baseURL, a.Val)
						if resolvedLink != "" {
							links = append(links, resolvedLink)
						}
					}
				}
			case "img":
				var imgSrc, imgAlt string
				for _, a := range n.Attr {
					if a.Key == "src" {
						imgSrc = resolveURL(baseURL, a.Val)
					} else if a.Key == "alt" {
						imgAlt = strings.TrimSpace(a.Val)
					}
				}
				if imgSrc != "" {
					images = append(images, ParsedImage{
						URL:     imgSrc,
						AltText: imgAlt,
					})
				}
			}
		} else if n.Type == html.TextNode {
			// Skip script and style tags
			parent := n.Parent
			if parent == nil || (parent.Data != "script" && parent.Data != "style" && parent.Data != "noscript" && parent.Data != "title") {
				cleanedText := strings.TrimSpace(n.Data)
				if cleanedText != "" {
					textBuilder.WriteString(cleanedText)
					textBuilder.WriteString(" ")
				}
			}
		}

		for c := n.FirstChild; c != nil; c = c.NextSibling {
			f(c)
		}
	}
	f(doc)

	return &ParsedPage{
		Title:   title,
		RawText: strings.TrimSpace(textBuilder.String()),
		Links:   cleanLinks(links),
		Images:  images,
	}, nil
}

func resolveURL(base *url.URL, ref string) string {
	refURL, err := url.Parse(ref)
	if err != nil {
		return ""
	}
	resolved := base.ResolveReference(refURL)
	// Only crawl http/https URLs
	if resolved.Scheme != "http" && resolved.Scheme != "https" {
		return ""
	}
	// Strip fragments
	resolved.Fragment = ""
	return resolved.String()
}

func cleanLinks(links []string) []string {
	seen := make(map[string]bool)
	var uniqueLinks []string
	for _, l := range links {
		if !seen[l] {
			seen[l] = true
			uniqueLinks = append(uniqueLinks, l)
		}
	}
	return uniqueLinks
}
